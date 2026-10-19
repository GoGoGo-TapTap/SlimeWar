// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeHUDWidget.h"

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"
#include "TimerManager.h"

void USlimeHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindRun();
	BindPlayer();
	UpdateAll();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyRefreshTimer,
			this,
			&USlimeHUDWidget::RefreshEcologySites,
			0.5f,
			/*bLoop=*/true);
	}
}

void USlimeHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EcologyRefreshTimer);
	}
	EcologyRefreshTimer.Invalidate();

	UnbindRun();
	UnbindPlayer();

	Super::NativeDestruct();
}

ASlimeRunGameState* USlimeHUDWidget::GetRunGameState() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetWorld()->GetGameState<ASlimeRunGameState>() : nullptr;
}

void USlimeHUDWidget::BindRun()
{
	UnbindRun();

	ASlimeRunGameState* RunState = GetRunGameState();
	if (!RunState)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("USlimeHUDWidget: no ASlimeRunGameState, the HUD will stay empty."));
		return;
	}

	RunState->OnScoreChanged.AddUObject(this, &USlimeHUDWidget::HandleScoreChanged);
	RunState->OnTimeChanged.AddUObject(this, &USlimeHUDWidget::HandleTimeChanged);
	RunState->OnFusionHint.AddUObject(this, &USlimeHUDWidget::HandleFusionHint);
	RunState->OnRunStateChanged.AddUObject(this, &USlimeHUDWidget::HandleRunStateChanged);
	RunState->OnRunEnded.AddUObject(this, &USlimeHUDWidget::HandleRunEnded);

	BoundGameState = RunState;
}

void USlimeHUDWidget::UnbindRun()
{
	if (ASlimeRunGameState* RunState = BoundGameState.Get())
	{
		RunState->OnScoreChanged.RemoveAll(this);
		RunState->OnTimeChanged.RemoveAll(this);
		RunState->OnFusionHint.RemoveAll(this);
		RunState->OnRunStateChanged.RemoveAll(this);
		RunState->OnRunEnded.RemoveAll(this);
	}

	BoundGameState.Reset();
}

void USlimeHUDWidget::BindPlayer()
{
	UnbindPlayer();

	// The HUD can be created a frame before the pawn is possessed, so the binding is retried by
	// UpdateAll rather than assumed to work here.
	const APlayerController* PC = GetOwningPlayer();
	ASlimeWarCharacter* Character = PC ? Cast<ASlimeWarCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}

	if (USlimeHealthComponent* Health = Character->GetHealthComponent())
	{
		Health->OnHealthChanged.AddDynamic(this, &USlimeHUDWidget::HandleHealthChanged);
		BoundHealth = Health;
	}

	if (USlimeWeaponComponent* Weapon = Character->GetWeaponComponent())
	{
		Weapon->OnAmmoChanged.AddDynamic(this, &USlimeHUDWidget::HandleAmmoChanged);
		BoundWeapon = Weapon;
	}

        Character->OnGrenadeInventoryChanged.AddDynamic(
                this, &USlimeHUDWidget::HandleGrenadeInventoryChanged);
        Character->OnGrenadeSelectionChanged.AddDynamic(
                this, &USlimeHUDWidget::HandleGrenadeSelectionChanged);
        BoundCharacter = Character;
}

void USlimeHUDWidget::UnbindPlayer()
{
	if (USlimeHealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &USlimeHUDWidget::HandleHealthChanged);
	}
	BoundHealth.Reset();

	if (USlimeWeaponComponent* Weapon = BoundWeapon.Get())
	{
		Weapon->OnAmmoChanged.RemoveDynamic(this, &USlimeHUDWidget::HandleAmmoChanged);
	}
	BoundWeapon.Reset();

        if (ASlimeWarCharacter* Character = BoundCharacter.Get())
        {
                Character->OnGrenadeInventoryChanged.RemoveDynamic(
                        this, &USlimeHUDWidget::HandleGrenadeInventoryChanged);
                Character->OnGrenadeSelectionChanged.RemoveDynamic(
                        this, &USlimeHUDWidget::HandleGrenadeSelectionChanged);
        }
        BoundCharacter.Reset();
}

void USlimeHUDWidget::UpdateAll()
{
	if (!BoundHealth.IsValid() || !BoundWeapon.IsValid())
	{
		BindPlayer();
	}

	const ASlimeRunGameState* RunState = BoundGameState.Get();
	if (!RunState)
	{
		return;
	}

	const int32 CurrentScore = RunState->GetCurrentScore();
	const int32 TargetScore = RunState->GetTargetScore();

	OnScoreUpdated(CurrentScore, TargetScore);
	OnTimeUpdated(RunState->GetRemainingSeconds());

	// Sync the fly-in baseline: showing the HUD is not a kill.
	LastKnownScore = RunState->GetCurrentScore();

	if (const USlimeHealthComponent* Health = BoundHealth.Get())
	{
		OnHealthUpdated(Health->GetHealth(), Health->GetMaxHealth());
	}

	if (const USlimeWeaponComponent* Weapon = BoundWeapon.Get())
	{
		OnAmmoUpdated(Weapon->GetMagazineAmmo(), Weapon->GetMagazineSize());
	}

        if (const ASlimeWarCharacter* Character = BoundCharacter.Get())
        {
                OnGrenadeInventoryUpdated(
                        Character->GetAssimilationGrenadeCount(),
                        Character->GetTornadoGrenadeCount());
                OnGrenadeSelectionUpdated(
                        Character->GetSelectedGrenadeType(),
                        Character->GetSelectedGrenadeFaction());
        }

	EvaluateHints();
	RefreshEcologySites();
}

void USlimeHUDWidget::EvaluateHints()
{
	const ASlimeRunGameState* RunState = BoundGameState.Get();
	if (!RunState || RunState->GetRunState() != ESlimeRunState::Running)
	{
		return;
	}

	// Design 7.5: first landing teaches movement and shooting.
	if (!bShownLandingHint)
	{
		bShownLandingHint = true;
		ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialLanding", "WASD to move, left mouse to fire, right mouse to aim, R to reload."));
	}

	if (!bShownTargetHint && RunState->IsTargetReached())
	{
		bShownTargetHint = true;
		ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialTarget", "Target reached - keep pushing for a higher score."));
	}
}

void USlimeHUDWidget::HandleScoreChanged(int32 NewScore)
{
	const ASlimeRunGameState* RunState = BoundGameState.Get();
	const int32 TargetScore = RunState ? RunState->GetTargetScore() : 0;

	OnScoreUpdated(NewScore, TargetScore);

	// PA-12: tell the WBP how much the last kill was worth so it can fly an icon in. The first
	// update after construction is a sync, not a kill, hence the "did it actually go up" test.
	const int32 Delta = NewScore - LastKnownScore;
	LastKnownScore = NewScore;

	if (Delta > 0)
	{
		OnScoreEarned(NewScore, Delta);
	}

	if (!bShownTargetHint && RunState && RunState->IsTargetReached())
	{
		bShownTargetHint = true;
		ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialTarget", "Target reached - keep pushing for a higher score."));
	}
}

void USlimeHUDWidget::HandleTimeChanged(int32 NewRemainingSeconds)
{
	OnTimeUpdated(NewRemainingSeconds);

	// Design 7.5: one reminder in the last 15 seconds. The constant is a design value, not a
	// tunable - it is spelled out here on purpose.
	if (!bShownLastSecondsHint && NewRemainingSeconds > 0 && NewRemainingSeconds <= 15)
	{
		bShownLastSecondsHint = true;
		ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialLastSeconds", "15 seconds left."));
	}
}

void USlimeHUDWidget::HandleFusionHint(FVector Location)
{
	// Design 7.5: "fused targets are worth more but take longer to kill".
	ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialFusion",
		"Fused slimes are worth more - and much harder to kill."));
}

void USlimeHUDWidget::HandleRunStateChanged(ESlimeRunState NewState)
{
	if (NewState == ESlimeRunState::Running)
	{
		UpdateAll();
	}
}

void USlimeHUDWidget::HandleRunEnded(ERunEndReason Reason)
{
	// Nothing to do: the result screen takes over. Kept so the binding list is symmetrical.
}

void USlimeHUDWidget::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	OnHealthUpdated(NewHealth, MaxHealth);
}

void USlimeHUDWidget::HandleAmmoChanged(int32 CurrentAmmo, int32 MagazineSize)
{
	OnAmmoUpdated(CurrentAmmo, MagazineSize);

	// Design 7.5 teaches that reloading does not stop you moving. There is no "reload started"
	// signal above the GAS layer (and the UI must stay free of GAS types), so the hint fires the
	// first time a magazine comes back to full - which is the moment the player sees it anyway.
	if (!bShownReloadHint && MagazineSize > 0 && CurrentAmmo >= MagazineSize)
	{
		bShownReloadHint = true;
		ShowTutorial(NSLOCTEXT("SlimeWar", "TutorialReload", "You can move and aim while reloading."));
	}
}

void USlimeHUDWidget::HandleGrenadeInventoryChanged(int32 Assimilation, int32 Tornado)
{
	OnGrenadeInventoryUpdated(Assimilation, Tornado);
}

void USlimeHUDWidget::HandleGrenadeSelectionChanged(
	ESlimeGrenadeType Type,
	ESlimeFaction Faction)
{
	OnGrenadeSelectionUpdated(Type, Faction);
}

void USlimeHUDWidget::RefreshEcologySites()
{
	UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (Ecology)
	{
		OnEcologySitesUpdated(Ecology->GetSiteStatuses());
	}
}
