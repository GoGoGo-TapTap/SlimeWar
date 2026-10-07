// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/SlimeWarCharacter.h"
#include "TimerManager.h"
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

	ApplyPhaseToScreens(Phase);
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
	auto CreateScreen = [PC](const TSoftClassPtr<UUserWidget>& WidgetClass, const TCHAR* Label) -> UUserWidget*
	{
		UClass* LoadedClass = WidgetClass.LoadSynchronous();
		if (!LoadedClass)
		{
			// Distinguish "nothing configured" from "a path that does not resolve yet": the second
			// case is the normal state while the WBP assets are still being made, and seeing the
			// actual path is what makes that obvious.
			UE_LOG(LogSlimeWar, Warning,
				TEXT("USlimeUISubsystem: %s widget class could not be loaded (%s); that screen is skipped."),
				Label,
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
	};

	HUDWidget = CreateScreen(Settings->HUDWidgetClass, TEXT("HUD"));
	PreparationWidget = CreateScreen(Settings->PreparationWidgetClass, TEXT("Preparation"));
	ResultWidget = CreateScreen(Settings->ResultWidgetClass, TEXT("Result"));
	PauseWidget = CreateScreen(Settings->PauseWidgetClass, TEXT("Pause"));
}

void USlimeUISubsystem::ApplyPhaseToScreens(ESlimeRunState Phase)
{
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
		Show(PreparationWidget, true);
		Show(HUDWidget, false);
		Show(ResultWidget, false);
		SetUIOnlyInput();
		break;

	case ESlimeRunState::Deploying:
	case ESlimeRunState::Result:
		// Cinematics: no screen at all, the world is the presentation.
		Show(PreparationWidget, false);
		Show(HUDWidget, false);
		Show(ResultWidget, false);
		SetGameInput();
		break;

	case ESlimeRunState::Running:
		Show(PreparationWidget, false);
		Show(ResultWidget, false);
		Show(HUDWidget, true);
		SetGameInput();
		break;

	case ESlimeRunState::Ended:
		Show(PreparationWidget, false);
		Show(HUDWidget, false);
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
	// The settlement screen appears on the Ended transition, after the orbit camera had its time.
	if (bPaused)
	{
		SetPauseState(false);
	}
}

void USlimeUISubsystem::ConfirmDropPoint(int32 DropPointIndex)
{
	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->BeginDeployment(DropPointIndex);
	}
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
	if (bPaused)
	{
		SetPauseState(false);
	}

	// No main menu exists in this project (design 1.5), so quitting is the only "exit".
	APlayerController* PC = GetPlayerController();
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
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
