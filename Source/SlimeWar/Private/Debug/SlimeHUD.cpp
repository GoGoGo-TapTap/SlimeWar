// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeHUD.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeWeakpointComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeFusionComponent.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeEnemyManagerSubsystem.h"
#include "Flow/SlimeFlowNames.h"
#include "Flow/SlimeRunGameState.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/StatTableProvider.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Player/SlimeAimAssistComponent.h"
#include "Player/SlimeWarCharacter.h"

namespace
{
	/** Debug-only drawing constants (not gameplay balance values). */
	constexpr float CrosshairGap = 4.f;
	constexpr float CrosshairLength = 10.f;
	constexpr float CrosshairThickness = 1.5f;
	constexpr float EnemyLabelHeight = 120.f;
	/** Fusion labels sit lower so they do not cover the enemy state label. */
	constexpr float FusionLabelHeight = 60.f;
	constexpr int32 ActivityCircleSegments = 24;
	constexpr int32 ContactCircleSegments = 20;

	/** Phase C run HUD layout (debug only). */
	constexpr float RunHudMargin = 24.f;
	constexpr float RunHudLineHeight = 15.f;
	constexpr float RunHudScale = 1.f;
	constexpr float RunHudTitleScale = 1.25f;

	/** Aggro chase debug (Slime.Debug.DrawAggroPath). All debug-only, not gameplay values. */
	constexpr float TrailSampleInterval = 0.1f;
	constexpr int32 TrailMaxPoints = 80;
	constexpr float TrailThickness = 2.f;
	constexpr float PathThickness = 1.5f;
	constexpr float PathPointRadius = 12.f;
	constexpr float AggroPathLabelHeight = 90.f;
	/** New ecology debug is multi-line: each status line uses this vertical step. */
	constexpr float EcologyStateLineHeight = 16.f;

	FString MoveStatusName(const EPathFollowingStatus::Type Status)
	{
		switch (Status)
		{
		case EPathFollowingStatus::Idle:	return TEXT("Idle");
		case EPathFollowingStatus::Waiting:	return TEXT("Waiting");
		case EPathFollowingStatus::Paused:	return TEXT("Paused");
		case EPathFollowingStatus::Moving:	return TEXT("Moving");
		default:							return TEXT("?");
		}
	}

	FString FusionStateName(const ESlimeFusionState State)
	{
		switch (State)
		{
		case ESlimeFusionState::Approaching: return TEXT("Approaching");
		case ESlimeFusionState::Contacting:  return TEXT("Contacting");
		case ESlimeFusionState::Cooling:     return TEXT("Cooling");
		default:                             return TEXT("Idle");
		}
	}

	FColor FusionStateColor(const ESlimeFusionState State)
	{
		switch (State)
		{
		case ESlimeFusionState::Approaching: return FColor(80, 180, 255);
		case ESlimeFusionState::Contacting:  return FColor(255, 210, 60);
		case ESlimeFusionState::Cooling:     return FColor(160, 160, 160);
		default:                             return FColor::White;
		}
	}

	FString EcologyFusionStateName(const ESlimeEcologyFusionState State)
	{
		switch (State)
		{
		case ESlimeEcologyFusionState::Seeking:		return TEXT("Seeking");
		case ESlimeEcologyFusionState::Contacting:	return TEXT("Contacting");
		case ESlimeEcologyFusionState::Cooldown:	return TEXT("Cooldown");
		case ESlimeEcologyFusionState::Blocked:		return TEXT("Blocked");
		default:									return TEXT("Idle");
		}
	}

	FString EcologyFusionPathName(const ESlimeEcologyFusionPath Path)
	{
		switch (Path)
		{
		case ESlimeEcologyFusionPath::Autonomous:	return TEXT("Autonomous");
		case ESlimeEcologyFusionPath::Contact:		return TEXT("Contact");
		case ESlimeEcologyFusionPath::Forced:		return TEXT("Forced");
		default:									return TEXT("None");
		}
	}

	FString EcologyBodyStateName(const ESlimeBodyState State)
	{
		switch (State)
		{
		case ESlimeBodyState::AimLocked:	return TEXT("AimLocked");
		case ESlimeBodyState::StunPending:	return TEXT("StunPending");
		case ESlimeBodyState::Stunned:		return TEXT("Stunned");
		default:							return TEXT("Normal");
		}
	}

	FString EcologyFactionName(const ESlimeFaction Faction)
	{
		switch (Faction)
		{
		case ESlimeFaction::B: return TEXT("B");
		case ESlimeFaction::C: return TEXT("C");
		default:				return TEXT("A");
		}
	}

	FString CoreStateName(const ESlimeCoreState State)
	{
		switch (State)
		{
		case ESlimeCoreState::Neutral: return TEXT("Neutral");
		case ESlimeCoreState::Hostile: return TEXT("Hostile");
		default:						return TEXT("None");
		}
	}

	FLinearColor EcologyFusionColor(
		const ESlimeEcologyFusionState FusionState,
		const ESlimeBodyState BodyState)
	{
		if (BodyState == ESlimeBodyState::Stunned || BodyState == ESlimeBodyState::StunPending)
		{
			return FLinearColor(1.f, 0.55f, 0.1f);
		}
		if (BodyState == ESlimeBodyState::AimLocked)
		{
			return FLinearColor(0.8f, 0.5f, 1.f);
		}

		switch (FusionState)
		{
		case ESlimeEcologyFusionState::Seeking:		return FLinearColor(0.3f, 0.75f, 1.f);
		case ESlimeEcologyFusionState::Contacting:	return FLinearColor(1.f, 0.85f, 0.2f);
		case ESlimeEcologyFusionState::Blocked:		return FLinearColor(1.f, 0.25f, 0.25f);
		case ESlimeEcologyFusionState::Cooldown:	return FLinearColor(0.65f, 0.65f, 0.65f);
		default:									return FLinearColor::White;
		}
	}
}

void ASlimeHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (SlimeCVars::DebugCrosshair != 0)
	{
		DrawCrosshair();
	}

	if (SlimeCVars::DebugDrawEnemyState != 0)
	{
		DrawEnemyStateDebug();
	}

	if (SlimeCVars::DebugDrawFusion != 0)
	{
		DrawFusionDebug();
	}

	if (SlimeCVars::DebugDrawEcologyState != 0)
	{
		DrawEcologyStateDebug();
	}

	if (SlimeCVars::DebugDrawAggroPath != 0)
	{
		DrawAggroPathDebug();
	}

	if (SlimeCVars::DebugDrawAimAssist != 0)
	{
		DrawWeaponDebug();
	}

	if (SlimeCVars::DebugDrawRun != 0)
	{
		DrawRunDebug();
	}

		DrawEcologyResourceDebug();
}

void ASlimeHUD::DrawWeaponDebug()
{
	const APlayerController* PC = GetOwningPlayerController();
	const ASlimeWarCharacter* Character =
		PC ? Cast<ASlimeWarCharacter>(PC->GetPawn()) : nullptr;
	const USlimeAimAssistComponent* Aim =
		Character ? Character->GetAimAssistComponent() : nullptr;
	if (!Character || !Aim || !Canvas)
	{
		return;
	}

	const float CentreX = Canvas->ClipX * 0.5f;
	const float CentreY = Canvas->ClipY * 0.5f;
	const float CaptureRadius = Aim->GetLastCaptureRadius();
	if (CaptureRadius > 0.f)
	{
		FVector2D Previous = FVector2D::ZeroVector;
		for (int32 Index = 0; Index <= 40; ++Index)
		{
			const float Angle = 2.f * PI * static_cast<float>(Index) / 40.f;
			const FVector2D Point(
				CentreX + FMath::Cos(Angle) * CaptureRadius,
				CentreY + FMath::Sin(Angle) * CaptureRadius);
			if (Index > 0)
			{
				DrawLine(Previous.X, Previous.Y, Point.X, Point.Y,
					FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f);
			}
			Previous = Point;
		}
	}

	if (Aim->GetLockedTarget())
	{
		const FVector Screen = Project(Aim->GetLockedAimPoint(), false);
		if (Screen.Z > 0.f)
		{
			const FLinearColor LockColor = Aim->IsWeakpointLocked()
				? FLinearColor(1.f, 0.85f, 0.1f)
				: FLinearColor(0.2f, 0.9f, 1.f);
			DrawRect(LockColor, Screen.X - 8.f, Screen.Y - 8.f, 16.f, 2.f);
			DrawRect(LockColor, Screen.X - 8.f, Screen.Y + 6.f, 16.f, 2.f);
			DrawRect(LockColor, Screen.X - 8.f, Screen.Y - 8.f, 2.f, 16.f);
			DrawRect(LockColor, Screen.X + 6.f, Screen.Y - 8.f, 2.f, 16.f);
			DrawText(
				Aim->IsWeakpointLocked() ? TEXT("LOCK WEAKPOINT") : TEXT("LOCK BODY"),
				LockColor,
				Screen.X + 12.f,
				Screen.Y - 8.f,
				nullptr,
				1.f);
		}
	}

	const float BarWidth = 220.f;
	const float BarHeight = 8.f;
	const float BarX = CentreX - BarWidth * 0.5f;
	const float BarY = Canvas->ClipY - 54.f;

	if (Character->IsReloading())
	{
		const float ReloadAlpha = Character->GetReloadProgress();
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BarX, BarY, BarWidth, BarHeight);
		DrawRect(FLinearColor(1.f, 0.65f, 0.15f, 0.95f),
			BarX, BarY, BarWidth * ReloadAlpha, BarHeight);
		DrawText(
			FString::Printf(TEXT("RELOADING %d%%"), FMath::RoundToInt(ReloadAlpha * 100.f)),
			FLinearColor::White,
			BarX,
			BarY - 16.f,
			nullptr,
			1.f);
	}
	else
	{
		const float ChargeAlpha = Character->GetChargeAlpha();
		if (Character->IsCharging() || ChargeAlpha > 0.f)
		{
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), BarX, BarY, BarWidth, BarHeight);
			DrawRect(FLinearColor(0.3f, 0.85f, 1.f, 0.95f),
				BarX, BarY, BarWidth * ChargeAlpha, BarHeight);
			DrawText(
				FString::Printf(TEXT("CHARGE %d%%"), FMath::RoundToInt(ChargeAlpha * 100.f)),
				FLinearColor::White,
				BarX,
				BarY - 16.f,
				nullptr,
				1.f);
		}
	}
}

void ASlimeHUD::DrawEcologyStateDebug()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	for (TActorIterator<ASlimeEcologyEnemy> It(World); It; ++It)
	{
		const ASlimeEcologyEnemy* Enemy = *It;
		const USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
		if (!Enemy || (Health && Health->IsDead()))
		{
			continue;
		}

		const USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent();
		const USlimeEcologyBrainComponent* Brain = Enemy->GetEcologyBrain();
		const USlimeBodyInteractionComponent* Body = Enemy->GetBodyInteraction();
		const USlimeCoreComponent* Core = Enemy->GetCoreComponent();
		const USlimeWeakpointComponent* Weakpoint = Enemy->GetWeakpointComponent();
		const ESlimeEcologyFusionState FusionState =
			Fusion ? Fusion->GetState() : ESlimeEcologyFusionState::Idle;
		const ESlimeBodyState BodyState =
			Body ? Body->GetBodyState() : ESlimeBodyState::Normal;
		const FLinearColor Color = EcologyFusionColor(FusionState, BodyState);
		const FLinearColor FinalColor = Core && Core->IsHostileCore()
			? FLinearColor(1.f, 0.35f, 0.25f)
			: Color;

		const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		const float LabelHeight =
			FMath::Max(EnemyLabelHeight, (Capsule ? Capsule->GetScaledCapsuleRadius() : 20.f) * 2.f + 85.f);
		const FVector Screen = Project(
			Enemy->GetActorLocation() + FVector(0.f, 0.f, LabelHeight),
			/*bClampToZeroPlane*/ false);
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		const FString PartnerName = Fusion && Fusion->GetPartner()
			? GetNameSafe(Fusion->GetPartner())
			: TEXT("None");
		const TArray<FString> Lines = {
			FString::Printf(TEXT("F%s  %s  mass %d  hp %.0f/%.0f  cargo %d  credit %d"),
				*EcologyFactionName(Enemy->GetFaction()),
				*GetNameSafe(Enemy),
				Enemy->GetMass(),
				Health ? Health->GetHealth() : 0.f,
				Health ? Health->GetMaxHealth() : 0.f,
				Enemy->GetCargo(),
				Enemy->GetBodyScoreCredit()),
			FString::Printf(TEXT("fusion %s/%s  contact %.0f%%  partner %s  ready %.1f  cd %.2f  block %.2f"),
				*EcologyFusionStateName(FusionState),
				*EcologyFusionPathName(Fusion ? Fusion->GetPath() : ESlimeEcologyFusionPath::None),
				Fusion ? Fusion->GetContactAlpha() * 100.f : 0.f,
				*PartnerName,
				Fusion ? Fusion->GetAutoReadyRemaining() : 0.f,
				Fusion ? Fusion->GetCooldownRemaining() : 0.f,
				Fusion ? Fusion->GetBlockRemaining() : 0.f),
			FString::Printf(TEXT("brain %s  body %s  external %.0f  stunP %.2f  stun %.2f  pushWin %.2f"),
				Brain ? Brain->GetBehaviorName() : TEXT("None"),
				*EcologyBodyStateName(BodyState),
				Body ? Body->GetExternalVelocity().Size2D() : 0.f,
				Body ? Body->GetStunPending() : 0.f,
				Body ? Body->GetStunRemaining() : 0.f,
				Body ? Body->GetForcedFusionWindowRemaining() : 0.f),
			FString::Printf(TEXT("velocity %.1f  position %s"),
				Enemy->GetVelocity().Size2D(),
				*Enemy->GetActorLocation().ToCompactString()),
			FString::Printf(TEXT("core %s/%s  target %s  wake %.2f  windup %.2f  pounce %.2f  cd %.2f"),
				*CoreStateName(Core ? Core->GetCoreState() : ESlimeCoreState::None),
				Core ? Core->GetPhaseName() : TEXT("None"),
				Core && Core->GetCombatTarget() ? *GetNameSafe(Core->GetCombatTarget()) : TEXT("None"),
				Core ? Core->GetWakeRemaining() : 0.f,
				Core ? Core->GetWindupRemaining() : 0.f,
				Core ? Core->GetPounceRemaining() : 0.f,
				Core ? Core->GetAttackCooldownRemaining() : 0.f)
		};

		float MaxTextWidth = 0.f;
		for (const FString& Line : Lines)
		{
			float TextWidth = 0.f;
			float TextHeight = 0.f;
			GetTextSize(Line, TextWidth, TextHeight, nullptr, 1.f);
			MaxTextWidth = FMath::Max(MaxTextWidth, TextWidth);
		}
		DrawRect(
			FLinearColor(0.f, 0.f, 0.f, 0.48f),
			Screen.X - 5.f,
			Screen.Y - 5.f,
			MaxTextWidth + 10.f,
			Lines.Num() * EcologyStateLineHeight + 10.f);

		float Y = Screen.Y;
		for (const FString& Line : Lines)
		{
			DrawText(Line, FinalColor, Screen.X, Y, nullptr, 1.f);
			Y += EcologyStateLineHeight;
		}

		if (Weakpoint && Weakpoint->IsWeakpointActive())
		{
			DrawDebugSphere(
				World,
				Weakpoint->GetComponentLocation(),
				Weakpoint->GetScaledSphereRadius(),
				12,
				FColor::Yellow,
				false,
				0.f,
				0,
				2.f);
		}
	}
}

void ASlimeHUD::DrawRunDebug()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	const ASlimeRunGameState* GameState = World->GetGameState<ASlimeRunGameState>();
	if (!GameState)
	{
		return;
	}

	const URunSubsystem* Run = URunSubsystem::Get(this);
	const USlimeEnemyManagerSubsystem* Manager = USlimeEnemyManagerSubsystem::Get(this);

	// -- top left: score / best / settlement counters --
	float Y = RunHudMargin;

	const FLinearColor ScoreColor = GameState->IsTargetReached()
		? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(1.f, 0.9f, 0.3f);

	DrawText(
		FString::Printf(TEXT("SCORE %d / %d"), GameState->GetCurrentScore(), GameState->GetTargetScore()),
		ScoreColor, RunHudMargin, Y, nullptr, RunHudTitleScale);
	Y += RunHudLineHeight * RunHudTitleScale;

	DrawText(
		FString::Printf(TEXT("BEST %d"), GameState->GetBestScore()),
		FLinearColor(0.8f, 0.8f, 0.8f), RunHudMargin, Y, nullptr, RunHudScale);
	Y += RunHudLineHeight;

	DrawText(
		FString::Printf(TEXT("kills %d   cleared %d"),
			GameState->GetNormalKillCount(), GameState->GetClearedPointCount()),
		FLinearColor(0.8f, 0.8f, 0.8f), RunHudMargin, Y, nullptr, RunHudScale);
	Y += RunHudLineHeight;

	if (Run)
	{
		const int32 LastBatch = Run->GetLastIssuedBatch();
		DrawText(
			FString::Printf(TEXT("run %s   batch %d/%d   fused %d"),
				SlimeFlowNames::RunState(Run->GetRunState()),
				LastBatch == INDEX_NONE ? 0 : LastBatch + 1,
				Run->GetBatchCount(),
				Run->GetEnemyFusedCount()),
			FLinearColor(0.7f, 0.7f, 0.9f), RunHudMargin, Y, nullptr, RunHudScale);
	}
	Y += RunHudLineHeight * 1.5f;

	// -- point states (the HUD marker PD-09 will replace this in Phase D) --
	for (int32 Index = 0; Index < GameState->GetPointCount(); ++Index)
	{
		const int32 PointId = GameState->GetPointIdAt(Index);
		const ESpawnPointState State = GameState->GetPointState(PointId);

		FLinearColor Color(0.8f, 0.8f, 0.8f);
		switch (State)
		{
		case ESpawnPointState::AwaitingDeploy:		Color = FLinearColor(0.6f, 0.6f, 0.6f); break;
		case ESpawnPointState::Spawning:			Color = FLinearColor(1.f, 0.8f, 0.3f); break;
		case ESpawnPointState::DepletedNotCleared:	Color = FLinearColor(1.f, 0.5f, 0.2f); break;
		case ESpawnPointState::Cleared:				Color = FLinearColor(0.4f, 1.f, 0.4f); break;
		default:									break;
		}

		DrawText(
			FString::Printf(TEXT("POINT %d  %s"), PointId, SlimeFlowNames::PointState(State)),
			Color, RunHudMargin, Y, nullptr, RunHudScale);
		Y += RunHudLineHeight;
	}

	// -- top right: countdown --
	const FString TimeText = FString::Printf(TEXT("TIME %d"), GameState->GetRemainingSeconds());
	float TextWidth = 0.f;
	float TextHeight = 0.f;
	GetTextSize(TimeText, TextWidth, TextHeight, nullptr, RunHudTitleScale);

	DrawText(TimeText, FLinearColor(1.f, 1.f, 1.f),
		Canvas->ClipX - TextWidth - RunHudMargin, RunHudMargin, nullptr, RunHudTitleScale);

	// -- bottom left: live / peak enemy counts (PE-05 input) --
	if (Manager)
	{
		DrawText(
			FString::Printf(TEXT("enemies live %d  (normal %d / aggro %d)   peak %d"),
				Manager->GetLiveEnemyCount(),
				Manager->GetLiveCount(ETargetKind::Normal),
				Manager->GetLiveCount(ETargetKind::Aggressive),
				Manager->GetPeakLiveEnemyCount()),
			FLinearColor(0.8f, 0.8f, 0.8f),
			RunHudMargin, Canvas->ClipY - RunHudMargin - RunHudLineHeight, nullptr, RunHudScale);
	}
}

void ASlimeHUD::DrawEcologyResourceDebug()
{
	UWorld* World = GetWorld();
	const USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Ecology || !Ecology->HasEcologySites() || !Canvas)
	{
		return;
	}

	const TArray<FSlimeSiteStatus> Sites = Ecology->GetSiteStatuses();
	const TArray<FSlimeMineralStatus> Minerals = Ecology->GetMineralStatuses();
	const FSlimeEcologyLedger Ledger = Ecology->BuildLedger();

	const float PanelWidth = 360.f;
	const float X = Canvas->ClipX - PanelWidth - RunHudMargin;
	float Y = 72.f;
	const float LineHeight = 16.f;

	DrawText(TEXT("ECOLOGY SITES"), FLinearColor(0.65f, 0.9f, 1.f),
		X, Y, nullptr, 1.1f);
	Y += LineHeight * 1.2f;

	for (const FSlimeSiteStatus& Site : Sites)
	{
		const FLinearColor Color = Site.Faction == ESlimeFaction::B
			? FLinearColor(0.95f, 0.7f, 0.25f)
			: FLinearColor(0.25f, 0.85f, 0.6f);
		DrawText(
			FString::Printf(TEXT("%s  live %d / mass %d  delivered %d  cores %d  pending %d"),
				*Site.SiteId.ToString(),
				Site.LiveCount,
				Site.LiveMass,
				Site.DeliveredCount,
				Site.CoreCount,
				Site.PendingSpawns),
			Color, X, Y, nullptr, RunHudScale);
		Y += LineHeight;
	}

	Y += 4.f;
	for (const FSlimeMineralStatus& Mineral : Minerals)
	{
		DrawText(
			FString::Printf(TEXT("%s stock %d/%d%s"),
				*Mineral.PatchId.ToString(),
				Mineral.Stock,
				Mineral.MaxStock,
				Mineral.bRegenerates
					? *FString::Printf(TEXT("  regen %.1fs"), Mineral.RegrowRemaining)
					: TEXT("")),
			FLinearColor(0.75f, 0.8f, 0.85f),
			X,
			Y,
			nullptr,
			0.9f);
		Y += LineHeight * 0.9f;
	}

	Y += 4.f;
	const FLinearColor LedgerColor = Ledger.IsResourceBalanced() && Ledger.IsMassBalanced()
		? FLinearColor(0.55f, 1.f, 0.55f)
		: FLinearColor(1.f, 0.35f, 0.25f);
	DrawText(
		FString::Printf(
			TEXT("resource %d+%d = %d+%d+%d+%d"),
			Ledger.InitialStock,
			Ledger.RegeneratedStock,
			Ledger.StockInPatches,
			Ledger.CargoCarried,
			Ledger.CargoOnGround,
			Ledger.DeliveredCargo),
		LedgerColor, X, Y, nullptr, 0.9f);
	Y += LineHeight * 0.9f;
	DrawText(
		FString::Printf(
			TEXT("mass %d+%d = %d+%d"),
			Ledger.InitialMass,
			Ledger.DeliveredMass,
			Ledger.LiveMass,
			Ledger.RemovedMass),
		LedgerColor, X, Y, nullptr, 0.9f);
}

void ASlimeHUD::DrawAggroPathDebug()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	PruneTrails();

	const float Now = World->GetTimeSeconds();

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		ASlimeEnemyBase* Enemy = *It;
		if (!Enemy || !Enemy->IsAggressive())
		{
			continue;
		}

		const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
		if (Health && Health->IsDead())
		{
			continue;
		}

		// -- the trail it actually walked: crowd avoidance never shows up in the nav path, so this
		//    is the only thing that answers "did it go around or grind in place?" --
		if (FSlimeTrail* Trail = FindOrAddTrail(*Enemy))
		{
			if (Now >= Trail->NextSampleTime)
			{
				Trail->Points.Add(Enemy->GetActorLocation());
				Trail->NextSampleTime = Now + TrailSampleInterval;

				if (Trail->Points.Num() > TrailMaxPoints)
				{
					Trail->Points.RemoveAt(0, Trail->Points.Num() - TrailMaxPoints, EAllowShrinking::No);
				}
			}

			// Oldest = dark grey, newest = orange.
			for (int32 Index = 1; Index < Trail->Points.Num(); ++Index)
			{
				const float Alpha = static_cast<float>(Index) / static_cast<float>(Trail->Points.Num());
				const FColor Color = FLinearColor::LerpUsingHSV(
					FLinearColor(0.15f, 0.15f, 0.15f), FLinearColor(1.f, 0.45f, 0.05f), Alpha).ToFColor(true);

				DrawDebugLine(World, Trail->Points[Index - 1], Trail->Points[Index],
					Color, false, -1.f, 0, TrailThickness);
			}
		}

		// -- the path it plans to walk (cyan): compare it with the trail above --
		const AAIController* Controller = Cast<AAIController>(Enemy->GetController());
		const UPathFollowingComponent* PathFollowing = Controller ? Controller->GetPathFollowingComponent() : nullptr;
		if (!PathFollowing)
		{
			continue;
		}

		const FNavPathSharedPtr Path = PathFollowing->GetPath();
		if (Path.IsValid())
		{
			const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
			for (int32 Index = 0; Index < PathPoints.Num(); ++Index)
			{
				DrawDebugSphere(World, PathPoints[Index].Location, PathPointRadius, 8,
					FColor::Cyan, false, -1.f, 0, 1.f);

				if (Index > 0)
				{
					DrawDebugLine(World, PathPoints[Index - 1].Location, PathPoints[Index].Location,
						FColor::Cyan, false, -1.f, 0, PathThickness);
				}
			}
		}

		// -- label: the move status is what separates "no path" from "following one" --
		const FVector Screen = Project(
			Enemy->GetActorLocation() + FVector(0.f, 0.f, AggroPathLabelHeight), /*bClampToZeroPlane*/ false);
		if (Screen.Z > 0.f)
		{
			const FString Label = FString::Printf(TEXT("%s   %s   %.0fcm/s"),
				*GetNameSafe(Enemy), *MoveStatusName(PathFollowing->GetStatus()), Enemy->GetVelocity().Size2D());

			DrawText(Label, FLinearColor(FColor::Cyan), Screen.X, Screen.Y, nullptr, 1.f);
		}
	}
}

ASlimeHUD::FSlimeTrail* ASlimeHUD::FindOrAddTrail(AActor& Actor)
{
	const TWeakObjectPtr<AActor> Weak(&Actor);
	if (!Weak.IsValid())
	{
		return nullptr;
	}

	for (FSlimeTrail& Trail : Trails)
	{
		if (Trail.Actor == Weak)
		{
			return &Trail;
		}
	}

	FSlimeTrail& NewTrail = Trails.AddDefaulted_GetRef();
	NewTrail.Actor = Weak;
	return &NewTrail;
}

void ASlimeHUD::PruneTrails()
{
	Trails.RemoveAll([](const FSlimeTrail& Trail) { return !Trail.Actor.IsValid(); });
}

void ASlimeHUD::DrawFusionDebug()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		ASlimeEnemyBase* Enemy = *It;
		if (!Enemy || Enemy->IsAggressive())
		{
			continue;
		}

		const USlimeFusionComponent* Fusion = Enemy->GetFusionComponent();
		if (!Fusion || !Fusion->IsEngaged())
		{
			continue;
		}

		const ESlimeFusionState State = Fusion->GetState();
		const FColor Color = FusionStateColor(State);
		const FVector Location = Enemy->GetActorLocation();
		const ASlimeEnemyBase* Partner = Fusion->GetPartner();

		// Meeting point both slimes walk to (shared by the pair, so only draw it once per pair).
		const FVector MeetingPoint = Fusion->GetMeetingPoint();
		if (!MeetingPoint.IsNearlyZero() && (!Partner || Enemy->GetUniqueID() < Partner->GetUniqueID()))
		{
			DrawDebugSphere(World, MeetingPoint, 30.f, 8, Color, false, -1.f, 0, 2.f);
		}

		if (Partner)
		{
			DrawDebugLine(World, Location, Partner->GetActorLocation(), Color, false, -1.f, 0, 3.f);

			// Contact ring: how close the two capsule centres have to get.
			DrawDebugSphere(World, Location, Fusion->GetContactDistance(Partner), ContactCircleSegments,
				Color, false, -1.f, 0, 1.f);
		}

		const FVector WorldLabel = Location + FVector(0.f, 0.f, FusionLabelHeight);
		const FVector Screen = Project(WorldLabel, /*bClampToZeroPlane*/ false);
		if (Screen.Z <= 0.f)
		{
			continue;
		}

		FString Label = FString::Printf(TEXT("fusion %s   contact %.0f%%"),
			*FusionStateName(State), Fusion->GetContactAlpha() * 100.f);

		if (State == ESlimeFusionState::Cooling)
		{
			Label += FString::Printf(TEXT("   retry in %.2fs"), Fusion->GetCooldownRemaining());
		}
		else if (Partner)
		{
			Label += FString::Printf(TEXT("   dist %.0fcm"), FVector::Dist(Location, Partner->GetActorLocation()));
		}

		DrawText(Label, FLinearColor(Color), Screen.X, Screen.Y, nullptr, 1.f);
	}
}

void ASlimeHUD::DrawCrosshair()
{
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const FLinearColor Color(1.f, 1.f, 1.f, 0.85f);

	// A four-stroke cross with a gap in the middle, so the exact aim point stays readable.
	DrawLine(CenterX - CrosshairGap - CrosshairLength, CenterY, CenterX - CrosshairGap, CenterY, Color, CrosshairThickness);
	DrawLine(CenterX + CrosshairGap, CenterY, CenterX + CrosshairGap + CrosshairLength, CenterY, Color, CrosshairThickness);
	DrawLine(CenterX, CenterY - CrosshairGap - CrosshairLength, CenterX, CenterY - CrosshairGap, Color, CrosshairThickness);
	DrawLine(CenterX, CenterY + CrosshairGap, CenterX, CenterY + CrosshairGap + CrosshairLength, Color, CrosshairThickness);
}

void ASlimeHUD::DrawEnemyStateDebug()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		const ASlimeEnemyBase* Enemy = *It;
		if (!Enemy)
		{
			continue;
		}

		const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
		if (Health && Health->IsDead())
		{
			continue;
		}

		const FVector WorldLabel = Enemy->GetActorLocation() + FVector(0.f, 0.f, EnemyLabelHeight);
		const FVector Screen = Project(WorldLabel, /*bClampToZeroPlane*/ false);
		if (Screen.Z <= 0.f)
		{
			// Behind the camera.
			continue;
		}

		// --- state tags ---
		FString Label;
		if (const USlimeStateComponent* State = Enemy->GetStateComponent())
		{
			// Not named "Tags": AActor already has a member called Tags and shadowing it is an error here.
			TArray<FGameplayTag> StateTagList;
			State->GetStateTags().GetGameplayTagArray(StateTagList);

			TArray<FString> TagNames;
			TagNames.Reserve(StateTagList.Num());
			for (const FGameplayTag& Tag : StateTagList)
			{
				TagNames.Add(Tag.GetTagName().ToString());
			}

			Label = TagNames.Num() > 0 ? FString::Join(TagNames, TEXT(" | ")) : TEXT("<no state tag>");
		}

		// Keep it single line: AHUD::DrawText does not reliably render embedded newlines.
		Label += FString::Printf(TEXT("   mass %d   hp %.0f"), Enemy->GetMass(),
			Health ? Health->GetHealth() : 0.f);

		if (Enemy->GetTargetKind() == ETargetKind::Normal)
		{
			if (const ASlimeEnemyBase* Target = Enemy->GetFusionTarget())
			{
				Label += FString::Printf(TEXT("   fusion -> mass %d"), Target->GetMass());
				DrawDebugLine(World, Enemy->GetActorLocation(), Target->GetActorLocation(),
					FColor::Green, false, -1.f, 0, 2.f);
			}
			else
			{
				Label += TEXT("   no fusion target");
			}
		}

		DrawText(Label, FLinearColor::Yellow, Screen.X, Screen.Y, nullptr, 1.f);

		// --- activity radius (normal slimes only) ---
		if (Enemy->GetTargetKind() != ETargetKind::Normal)
		{
			continue;
		}

		const FVector Center = Enemy->GetActivityCenter();
		if (Center.IsNearlyZero())
		{
			continue;
		}

		// Draw what the AI actually enforces: the slime's own point radius, with the global value as
		// the fallback for actors that were not spawned by a point (cheats, hand-placed slimes).
		float Radius = Enemy->GetActivityRadius();
		if (Radius <= 0.f)
		{
			UGameInstance* GameInstance = World->GetGameInstance();
			UStatTableProvider* Provider =
				GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
			const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;
			Radius = RunConfig ? RunConfig->AIActivityRadius : 0.f;
		}

		if (Radius <= 0.f)
		{
			continue;
		}

		FVector PreviousScreen = FVector::ZeroVector;
		for (int32 Segment = 0; Segment <= ActivityCircleSegments; ++Segment)
		{
			const float Angle = 2.f * PI * static_cast<float>(Segment) / ActivityCircleSegments;
			const FVector WorldPoint = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
			const FVector ScreenPoint = Project(WorldPoint, /*bClampToZeroPlane*/ false);

			if (ScreenPoint.Z > 0.f && !PreviousScreen.IsZero())
			{
				DrawLine(PreviousScreen.X, PreviousScreen.Y, ScreenPoint.X, ScreenPoint.Y,
					FLinearColor(0.2f, 0.6f, 1.f, 0.6f), 1.f);
			}

			PreviousScreen = ScreenPoint.Z > 0.f ? ScreenPoint : FVector::ZeroVector;
		}
	}
}
