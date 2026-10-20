// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeSessionSubsystem.h"
#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeLevelDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SlimeWarCharacter.h"
#include "TimerManager.h"
#include "UI/SlimeAimPreviewWidget.h"
#include "UI/SlimeLevelSelectWidget.h"
#include "UI/SlimeMainMenuWidget.h"
#include "UI/SlimeResultWidget.h"

USlimeUISubsystem* USlimeUISubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<USlimeUISubsystem>() : nullptr;
}

void USlimeUISubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The GameState, the pawn and the UI settings are not all up yet: the game mode spawns the
	// GameState during StartPlay, and URunSubsystem pushes its first phase on the next tick. Waiting
	// one tick means this binding sees the final starting phase instead of racing it.
	InWorld.GetTimerManager().SetTimerForNextTick(this, &USlimeUISubsystem::SetupUI);
}

void USlimeUISubsystem::Deinitialize()
{
	if (ASlimeRunGameState* RunState = BoundGameState.Get())
	{
		RunState->OnRunStateChanged.RemoveAll(this);
		RunState->OnRunEnded.RemoveAll(this);
	}
	BoundGameState.Reset();

	if (ASlimeWarCharacter* Character = BoundCharacter.Get())
	{
		Character->OnPauseRequested.RemoveDynamic(this, &USlimeUISubsystem::HandlePauseRequested);
	}
	BoundCharacter.Reset();

	// Never leave the game paused behind us (a level reload during a pause would otherwise inherit
	// the paused flag). This deliberately does not go through SetPauseState: that touches the
	// widgets, and they are already being torn down at this point.
	if (bPaused)
	{
		bPaused = false;
		if (APlayerController* PC = GetPlayerController())
		{
			PC->SetPause(false);
		}
	}

	Super::Deinitialize();
}

APlayerController* USlimeUISubsystem::GetPlayerController() const
{
	UWorld* World = GetWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}

ASlimeRunGameState* USlimeUISubsystem::GetRunGameState() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameState<ASlimeRunGameState>() : nullptr;
}

ASlimeWarCharacter* USlimeUISubsystem::GetPlayerCharacter() const
{
	const APlayerController* PC = GetPlayerController();
	return PC ? Cast<ASlimeWarCharacter>(PC->GetPawn()) : nullptr;
}

void USlimeUISubsystem::SetupUI()
{
	if (ASlimeRunGameState* RunState = GetRunGameState())
	{
		RunState->OnRunStateChanged.AddUObject(this, &USlimeUISubsystem::HandleRunStateChanged);
		RunState->OnRunEnded.AddUObject(this, &USlimeUISubsystem::HandleRunEnded);
		BoundGameState = RunState;
	}
	else
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("USlimeUISubsystem: no ASlimeRunGameState; the screens cannot follow the run."));
	}

	// Bind the pause key. The character only reports it; what "pause" means is decided here.
	if (ASlimeWarCharacter* Character = GetPlayerCharacter())
	{
		Character->OnPauseRequested.AddDynamic(this, &USlimeUISubsystem::HandlePauseRequested);
		BoundCharacter = Character;
	}

	EnsureWidgets();

	USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);

	// "Leave the run" is the only request that may override the editor's PIE shortcut, so it is
	// consumed before ShouldEnterGameplay() is asked anything.
	const bool bMainMenuRequested = Session && Session->ConsumeMainMenuRequest();

	if (bMainMenuRequested || !ShouldEnterGameplay())
	{
		ApplyMenuState(ESlimeUIFlowState::MainMenu);
		return;
	}

	if (Session)
	{
		Session->ConsumeGameplayEntryRequest();
	}

	// Before the "no preparation screen" fallback below: that check has to see the real widget, or
	// every run would silently deploy to drop point 0.
	EnsurePreparationWidget();

	const ESlimeRunState Phase = BoundGameState.IsValid() ? BoundGameState->GetRunState() : ESlimeRunState::Idle;

	// Degrade instead of dead-ending.
	//
	// The preparation screen is the ONLY way into a run, so a missing preparation widget would leave
	// the player frozen in Idle with nothing to click - which reads as "the game is broken" while the
	// WBP assets are still being authored. Deploy to the first drop point instead, and if there is no
	// usable drop point either, start the timeline directly so gameplay stays reachable.
	if (!PreparationWidget && Phase == ESlimeRunState::Idle)
	{
		URunSubsystem* Run = URunSubsystem::Get(this);

		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeUISubsystem: no preparation screen, so a drop point cannot be chosen. Deploying to ")
			TEXT("drop point 0 so the run is still reachable."));

		if (!Run || !Run->BeginDeployment(0))
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("USlimeUISubsystem: drop point 0 is not usable either (is DA_SpawnLayout::DropPoints empty?). ")
				TEXT("Starting the run timeline directly."));

			if (Run)
			{
				Run->StartRun();
			}
		}

		// BeginDeployment / StartRun already pushed the new phase through the delegate.
		return;
	}

	// A same-point retry reloads straight into Deploying, but URunSubsystem and this subsystem both
	// run on next tick and the order is not guaranteed. Showing the preparation screen here would
	// flash it for a frame when this one wins.
	//
	// Waiting one more tick instead of returning outright: by then the deployment has either started
	// (nothing to show) or failed (the preparation screen MUST appear, or the player sits in front of
	// an empty screen with no way into the run).
	if (Session && Session->HasAutoDeployRequest() && Phase == ESlimeRunState::Idle)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &USlimeUISubsystem::ApplyBoundPhaseToScreens);
		}
		return;
	}

	ApplyPhaseToScreens(Phase);
}

void USlimeUISubsystem::ApplyBoundPhaseToScreens()
{
	ApplyPhaseToScreens(BoundGameState.IsValid() ? BoundGameState->GetRunState() : ESlimeRunState::Idle);
}

void USlimeUISubsystem::EnsureWidgets()
{
	if (bWidgetsCreated)
	{
		return;
	}

	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	bWidgetsCreated = true;

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Settings)
	{
		return;
	}

	// A missing class is a supported state: the code side ships before the WBP assets do, and the
	// run still plays (only that screen is skipped).
	MainMenuWidget = CreateScreenWidget(Settings->MainMenuWidgetClass, TEXT("MainMenu"));
	LevelSelectWidget = CreateScreenWidget(Settings->LevelSelectWidgetClass, TEXT("LevelSelect"));

	// A menu world only ever shows the two screens above; loading the HUD, the settlement screen and
	// the pause menu there is pure waste (each one is a class load plus a widget). They are created
	// in the world that will actually run the game.
	if (ShouldEnterGameplay())
	{
		HUDWidget = CreateScreenWidget(Settings->HUDWidgetClass, TEXT("HUD"));
		ResultWidget = CreateScreenWidget(Settings->ResultWidgetClass, TEXT("Result"));
		PauseWidget = CreateScreenWidget(Settings->PauseWidgetClass, TEXT("Pause"));

		// The aim overlay is optional: a project without the WBP (or without the C++ parent
		// reparented) simply plays without a crosshair.
		EnsureAimPreviewWidget();
	}

	// The drop point screen is deliberately absent: its class depends on the level, so it is created
	// by EnsurePreparationWidget() the first time a phase actually needs it.
}

UUserWidget* USlimeUISubsystem::CreateScreenWidget(const TSoftClassPtr<UUserWidget>& WidgetClass, const FString& Label)
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		// Distinguish "nothing configured" from "a path that does not resolve yet": the second case
		// is the normal state while the WBP assets are still being made, and seeing the actual path
		// is what makes that obvious.
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeUISubsystem: %s widget class could not be loaded (%s); that screen is skipped."),
			*Label,
			WidgetClass.IsNull()
				? TEXT("not set in Project Settings -> Game -> Slime War")
				: *WidgetClass.ToString());
		return nullptr;
	}

	TSubclassOf<UUserWidget> Class = LoadedClass;
	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, Class);
	if (Widget)
	{
		Widget->AddToViewport(0);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Widget;
}

void USlimeUISubsystem::EnsurePreparationWidget()
{
	if (PreparationWidget)
	{
		return;
	}

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Settings)
	{
		return;
	}

	// Level first, global second. A level that does not configure a drop point screen keeps working
	// with the single screen the project shipped with, which also covers PIE opened on a bare map.
	TSoftClassPtr<UUserWidget> WidgetClass = Settings->PreparationWidgetClass;
	FName LevelId = NAME_None;

	if (const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this))
	{
		LevelId = Session->GetSelectedLevelId();

		if (const USlimeLevelDefinition* Level = Session->GetSelectedLevelDefinition())
		{
			if (!Level->PreparationWidgetClass.IsNull())
			{
				WidgetClass = Level->PreparationWidgetClass;
			}
		}
	}

	PreparationWidget = CreateScreenWidget(WidgetClass, FString::Printf(TEXT("Preparation[%s]"), *LevelId.ToString()));

	if (!PreparationWidget)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeUISubsystem: no drop point screen for level '%s'; the run will fall back to drop point 0."),
			*LevelId.ToString());
	}
}

void USlimeUISubsystem::EnsureAimPreviewWidget()
{
	if (AimPreviewWidget)
	{
		return;
	}

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Settings || Settings->AimPreviewWidgetClass.IsNull())
	{
		// Optional screen: no crosshair is a supported state.
		return;
	}

	UUserWidget* Widget = CreateScreenWidget(Settings->AimPreviewWidgetClass, TEXT("AimPreview"));
	if (!Widget)
	{
		return;
	}

	// The parent class is what drives the ring and the crosshairs. A WBP still parented to plain
	// UUserWidget would be a static overlay, which is worse than no overlay at all.
	if (!Cast<USlimeAimPreviewWidget>(Widget))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeUISubsystem: AimPreviewWidgetClass is not a USlimeAimPreviewWidget. ")
			TEXT("Reparent WBP_AimPreview to SlimeAimPreviewWidget, or clear AimPreviewWidgetClass. ")
			TEXT("The aim overlay is disabled until then."));
		Widget->RemoveFromParent();
		return;
	}

	AimPreviewWidget = Widget;
}

bool USlimeUISubsystem::ShouldEnterGameplay() const
{
	const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);

	// An explicit "show the main menu" request beats everything below, including the PIE shortcut.
	// This is checked before SetupUI consumes the request, so both callers see the same answer.
	if (Session && Session->HasMainMenuRequest())
	{
		return false;
	}

	if (Session && (Session->HasGameplayEntryRequest() || Session->HasAutoDeployRequest()))
	{
		return true;
	}

	// The two flags above are ONE-SHOT: URunSubsystem runs on the same next tick and may call
	// ConsumeAutoDeployRequest() first, in which case this function would see nothing and fall back
	// to the main menu. A retry then reloads into an empty screen: the main menu screen covers the
	// deployment cinematic, and once it ends there is no HUD because none was ever created.
	//
	// So the answer must come from state nobody consumes.
	//
	// (1) The run phase. If the world is already deploying/running/settling, it is by definition
	//     a gameplay world - the menu world never leaves Idle.
	if (BoundGameState.IsValid() && BoundGameState->GetRunState() != ESlimeRunState::Idle)
	{
		return true;
	}

	// (2) The level identity. Nobody consumes it either, and it survives OpenLevel. Note that it
	//     stays None while USlimeGameSettings::Levels is empty (the picker falls back to "reload the
	//     current map" and never calls SetSelectedLevelId), so this only answers for configured
	//     levels - (1) is what covers the unconfigured case.
	if (Session && !Session->GetSelectedLevelId().IsNone())
	{
		return true;
	}

#if WITH_EDITOR
	// Opt-in shortcut for debugging a single map's combat or drop point screen. Off by default so a
	// PIE session starts exactly like a packaged build does - see
	// USlimeGameSettings::bSkipMainMenuInPIE.
	if (GetWorld() && GetWorld()->WorldType == EWorldType::PIE)
	{
		const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
		if (Settings && Settings->bSkipMainMenuInPIE)
		{
			return true;
		}
	}
#endif

	return false;
}

void USlimeUISubsystem::ApplyMenuState(ESlimeUIFlowState NewState)
{
	auto Hide = [](UUserWidget* Widget)
	{
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	};

	Hide(MainMenuWidget);
	Hide(LevelSelectWidget);
	Hide(PreparationWidget);
	Hide(HUDWidget);
	Hide(AimPreviewWidget);
	Hide(ResultWidget);
	Hide(PauseWidget);

	UIFlowState = NewState;

	switch (NewState)
	{
	case ESlimeUIFlowState::MainMenu:
		if (MainMenuWidget)
		{
			MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			UE_LOG(LogSlimeWar, Error,
				TEXT("USlimeUISubsystem: MainMenuWidget is unavailable; the game cannot present its entry screen."));
		}
		SetUIOnlyInput();
		break;

	case ESlimeUIFlowState::LevelSelect:
		if (LevelSelectWidget)
		{
			LevelSelectWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			UE_LOG(LogSlimeWar, Error,
				TEXT("USlimeUISubsystem: LevelSelectWidget is unavailable; returning to the main menu."));
			ApplyMenuState(ESlimeUIFlowState::MainMenu);
			return;
		}
		SetUIOnlyInput();
		break;

	default:
		break;
	}
}

void USlimeUISubsystem::ApplyPhaseToScreens(ESlimeRunState Phase)
{
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LevelSelectWidget)
	{
		LevelSelectWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PauseWidget)
	{
		PauseWidget->SetVisibility(bPaused ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (bPaused)
	{
		// The pause menu owns the screen until it is dismissed.
		return;
	}

	auto Show = [](UUserWidget* Widget, bool bVisible)
	{
		if (Widget)
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	};

	switch (Phase)
	{
	case ESlimeRunState::Idle:
		UIFlowState = ESlimeUIFlowState::Preparation;
		// "Reselect drop point" reaches this branch through the phase broadcast, not through
		// SetupUI, so the screen is created here too.
		EnsurePreparationWidget();
		Show(PreparationWidget, true);
		Show(HUDWidget, false);
		Show(AimPreviewWidget, false);
		Show(ResultWidget, false);
		SetUIOnlyInput();
		break;

	case ESlimeRunState::Deploying:
		UIFlowState = ESlimeUIFlowState::Deploying;
		// Cinematics: no screen at all, the world is the presentation.
		Show(PreparationWidget, false);
		Show(HUDWidget, false);
		Show(AimPreviewWidget, false);
		Show(ResultWidget, false);
		SetGameInput();
		break;

	case ESlimeRunState::Result:
		UIFlowState = ESlimeUIFlowState::Result;
		// Cinematics: no screen at all, the world is the presentation.
		Show(PreparationWidget, false);
		Show(HUDWidget, false);
		Show(AimPreviewWidget, false);
		Show(ResultWidget, false);
		SetGameInput();
		break;

	case ESlimeRunState::Running:
		UIFlowState = ESlimeUIFlowState::Running;
		Show(PreparationWidget, false);
		Show(ResultWidget, false);
		Show(HUDWidget, true);
		Show(AimPreviewWidget, true);
		SetGameInput();
		break;

	case ESlimeRunState::Ended:
		UIFlowState = ESlimeUIFlowState::Result;
		Show(PreparationWidget, false);
		Show(HUDWidget, false);
		Show(AimPreviewWidget, false);
		Show(ResultWidget, true);
		SetUIOnlyInput();

		// The widget was created at level start, so its construct-time pull happened long before
		// the run ended: refresh it now that the numbers are final and the screen is up.
		if (USlimeResultWidget* Result = Cast<USlimeResultWidget>(ResultWidget))
		{
			Result->RefreshResult();
		}
		break;

	default:
		break;
	}
}

void USlimeUISubsystem::HandleRunStateChanged(ESlimeRunState NewState)
{
	// A run that starts while the pause menu is open must not leave the game paused.
	if (bPaused && NewState != ESlimeRunState::Running)
	{
		SetPauseState(false);
	}

	ApplyPhaseToScreens(NewState);
}

void USlimeUISubsystem::HandleRunEnded(ERunEndReason Reason)
{
	// The phase change to Ended is what shows the settlement screen (ApplyPhaseToScreens). This
	// handler only guarantees the run does not end behind a frozen world: if the player was in the
	// pause menu when the timer ran out, the pause has to be lifted or the result screen appears
	// over a game stuck at zero time dilation.
	if (bPaused)
	{
		SetPauseState(false);
	}
}

void USlimeUISubsystem::ConfirmDropPoint(int32 DropPointIndex)
{
	URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeUISubsystem::ConfirmDropPoint: no run subsystem, drop point %d ignored."),
			DropPointIndex);
		return;
	}

	// Level id in the log on purpose: with one drop point screen per level, "drop point 1" alone is
	// ambiguous in a multi-level bug report.
	UE_LOG(LogSlimeWar, Log,
		TEXT("USlimeUISubsystem: confirming drop point %d for level '%s'."),
		DropPointIndex, *GetCurrentLevelId().ToString());

	Run->BeginDeployment(DropPointIndex);
}

FName USlimeUISubsystem::GetCurrentLevelId() const
{
	const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
	return Session ? Session->GetSelectedLevelId() : NAME_None;
}

void USlimeUISubsystem::OpenLevelSelect()
{
	if (!MainMenuWidget || !LevelSelectWidget)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeUISubsystem::OpenLevelSelect: menu widgets are not available."));
		return;
	}

	ApplyMenuState(ESlimeUIFlowState::LevelSelect);
}

void USlimeUISubsystem::ShowMainMenu()
{
	// Screen switch only. Menu-to-menu navigation must not touch the world: a reload here reads as
	// a freeze (the level can be minutes of streaming), and it also throws away the run systems for
	// nothing. Leaving a run is ReturnToMainMenu() below.
	ApplyMenuState(ESlimeUIFlowState::MainMenu);
}

void USlimeUISubsystem::ReturnToMainMenu()
{
	if (bPaused)
	{
		SetPauseState(false);
	}

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
	if (LevelName.IsEmpty())
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeUISubsystem::ReturnToMainMenu: could not resolve the current level name."));
		return;
	}

	if (USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this))
	{
		Session->ClearSelectedDropPoint();
		Session->ClearSelectedLevelId();

		// Without this the reloaded world would look like a fresh launch and the editor PIE
		// shortcut (USlimeGameSettings::bSkipMainMenuInPIE) would send the player straight back
		// into the level - i.e. "quit to main menu" would do nothing.
		Session->RequestMainMenu();
	}

	UE_LOG(LogSlimeWar, Log, TEXT("USlimeUISubsystem: returning to the main menu by reloading '%s'."), *LevelName);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void USlimeUISubsystem::RequestRetry()
{
	// Leave the pause before reloading: a reloaded level must not inherit the frozen time dilation.
	if (bPaused)
	{
		SetPauseState(false);
	}

	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->RequestRetry(/*bReselectDropPoint=*/false);
	}
}

void USlimeUISubsystem::RequestReselectDropPoint()
{
	if (bPaused)
	{
		SetPauseState(false);
	}

	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->RequestRetry(/*bReselectDropPoint=*/true);
	}
}

void USlimeUISubsystem::RequestQuit()
{
	ReturnToMainMenu();
}

int32 USlimeUISubsystem::GetCurrentLevelIndex() const
{
	const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Session || !Settings)
	{
		return INDEX_NONE;
	}

	return Settings->IndexOfLevel(Session->GetSelectedLevelId());
}

bool USlimeUISubsystem::HasNextLevel() const
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	const int32 Current = GetCurrentLevelIndex();
	if (!Settings || Current == INDEX_NONE)
	{
		return false;
	}

	// A slot may exist and still be unusable (no map), in which case the button must be grey.
	const USlimeLevelDefinition* Next = Settings->FindLevelByIndex(Current + 1);
	return Next != nullptr && !Next->MapAsset.IsNull();
}

bool USlimeUISubsystem::RequestNextLevel()
{
	if (bPaused)
	{
		SetPauseState(false);
	}

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	const int32 Current = GetCurrentLevelIndex();
	if (!Settings || Current == INDEX_NONE)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("USlimeUISubsystem::RequestNextLevel: no level identity (PIE on a bare map?), ignored."));
		return false;
	}

	const USlimeLevelDefinition* Next = Settings->FindLevelByIndex(Current + 1);
	if (!Next || Next->MapAsset.IsNull())
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("USlimeUISubsystem::RequestNextLevel: slot %d has no usable level."), Current + 1);
		return false;
	}

	const FString MapPackage = Next->MapAsset.GetLongPackageName();
	if (MapPackage.IsEmpty())
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeUISubsystem::RequestNextLevel: level %s has an unresolvable map path."),
			*Next->LevelId.ToString());
		return false;
	}

	USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
	if (!Session)
	{
		return false;
	}

	// The drop point index cannot carry over: the next level may have a different number of points,
	// so its own preparation screen has to ask again.
	Session->SetSelectedLevelId(Next->LevelId);
	Session->ClearSelectedDropPoint();
	Session->RequestEnterGameplay();

	UE_LOG(LogSlimeWar, Log, TEXT("USlimeUISubsystem: next level %s (%s)."),
		*Next->LevelId.ToString(), *MapPackage);

	UGameplayStatics::OpenLevel(this, FName(*MapPackage));
	return true;
}

void USlimeUISubsystem::RequestLevelSelect()
{
	if (bPaused)
	{
		SetPauseState(false);
	}

	if (!LevelSelectWidget)
	{
		// Without a picker the player would be stuck on the settlement screen: fall back to the one
		// screen that always exists.
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeUISubsystem::RequestLevelSelect: no level select widget, falling back to the main menu."));
		ReturnToMainMenu();
		return;
	}

	// Screen switch only. Reloading the world here would make "select level" a several-second
	// black screen; the reload happens when a level is actually picked.
	ApplyMenuState(ESlimeUIFlowState::LevelSelect);
}

void USlimeUISubsystem::HandlePauseRequested()
{
	TogglePause();
}

void USlimeUISubsystem::TogglePause()
{
	// Pausing only makes sense inside a run.
	if (!bPaused && (!BoundGameState.IsValid() || BoundGameState->GetRunState() != ESlimeRunState::Running))
	{
		return;
	}

	SetPauseState(!bPaused);
}

void USlimeUISubsystem::Resume()
{
	SetPauseState(false);
}

void USlimeUISubsystem::SetPauseState(bool bNewPaused)
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	bPaused = bNewPaused;

	// The world is frozen with a zero time dilation rather than an engine pause.
	//
	// An engine pause needs APlayerController::bShouldPerformFullTickWhenPaused (protected) to keep
	// routing input to the menu, and getting that wrong is exactly the "paused but the buttons do
	// not respond" bug. A zero dilation freezes movement, AI, fusion and the run timeline (world
	// timers scale with it) while Slate - and therefore the menu - stays fully alive.
	UGameplayStatics::SetGlobalTimeDilation(this, bPaused ? 0.f : 1.f);

	// Firing is an event, not a delta-gated action, so the freeze alone would still let the player
	// shoot; the run input gate is what closes it.
	if (ASlimeWarCharacter* Character = GetPlayerCharacter())
	{
		// Only a running phase may hand control back: unpausing because the run ended must not
		// undo the State.Player.Result lock that URunSubsystem just applied.
		if (bPaused)
		{
			Character->SetRunInputBlocked(true);
		}
		else if (!BoundGameState.IsValid() || BoundGameState->GetRunState() == ESlimeRunState::Running)
		{
			Character->SetRunInputBlocked(false);
		}
	}

	if (PauseWidget)
	{
		PauseWidget->SetVisibility(bPaused ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (bPaused)
	{
		UIFlowState = ESlimeUIFlowState::Pause;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		// Restore whatever the current phase wants (the HUD during a run, UI elsewhere).
		ApplyPhaseToScreens(BoundGameState.IsValid() ? BoundGameState->GetRunState() : ESlimeRunState::Running);
	}
}

void USlimeUISubsystem::SetUIOnlyInput()
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
}

void USlimeUISubsystem::SetGameInput()
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);
}
