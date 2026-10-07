// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeHUD.h"

#include "AIController.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarCoreTypes.h"
#include "DrawDebugHelpers.h"
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
#include "Navigation/PathFollowingComponent.h"

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

	if (SlimeCVars::DebugDrawAggroPath != 0)
	{
		DrawAggroPathDebug();
	}

	if (SlimeCVars::DebugDrawRun != 0)
	{
		DrawRunDebug();
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
