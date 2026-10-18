// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeCheatManager.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Enemy/SlimeAggro.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeFusionComponent.h"
#include "Enemy/SlimeNormal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/RunSubsystem.h"
#include "Flow/ScoreSubsystem.h"
#include "Flow/SlimeEnemyManagerSubsystem.h"
#include "Flow/SlimeFlowNames.h"
#include "Flow/SpawnPoint.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Math/NumericLimits.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UI/SlimeUISubsystem.h"

namespace
{
	/** Spawn distance / spacing are debug-only helpers, never gameplay balance values. */
	constexpr float SlimeCheatSpawnDistance = 500.f;
	constexpr float SlimeCheatSpawnSpacing = 150.f;
	/** Tight cluster used by the CP-2 fusion scenes so the slimes touch quickly. */
	constexpr float SlimeCheatFusionSpacing = 120.f;

	FVector GetCheatSpawnBase(const APlayerController* PlayerController, const int32 Index, const int32 Count, const float Spacing)
	{
		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		const FVector Forward = PlayerPawn ? PlayerPawn->GetActorForwardVector() : FVector::ForwardVector;
		const FVector Origin = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
		const FVector Right = PlayerPawn ? PlayerPawn->GetActorRightVector() : FVector::RightVector;

		const float Offset = (Index - (Count - 1) * 0.5f) * Spacing;
		return Origin + Forward * SlimeCheatSpawnDistance + Right * Offset;
	}

	TArray<ASlimeEnemyBase*> SpawnSlimes(
		UWorld* World,
		APlayerController* PlayerController,
		const TSubclassOf<ASlimeEnemyBase>& SlimeClass,
		const int32 Count,
		const float Spacing = SlimeCheatSpawnSpacing)
	{
		TArray<ASlimeEnemyBase*> Spawned;
		if (!World || !SlimeClass || Count <= 0)
		{
			return Spawned;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawned.Reserve(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Location = GetCheatSpawnBase(PlayerController, Index, Count, Spacing);

			// Spawned pawns auto-possess ASlimeAIController (see ASlimeEnemyBase),
			// which starts the StateTree, so the activity centre is set right after.
			if (ASlimeEnemyBase* Slime = World->SpawnActor<ASlimeEnemyBase>(
				SlimeClass, Location, FRotator::ZeroRotator, SpawnParameters))
			{
				Slime->InitializeFromSpawn(0, Location);
				Spawned.Add(Slime);
			}
		}

		return Spawned;
	}

	/** Nearest live normal slime to the player, or null. */
	ASlimeEnemyBase* FindNearestNormalSlime(UWorld* World, const APlayerController* PlayerController)
	{
		if (!World)
		{
			return nullptr;
		}

		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		const FVector Origin = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

		ASlimeEnemyBase* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();

		for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
		{
			ASlimeEnemyBase* Enemy = *It;
			if (!Enemy || Enemy->IsAggressive() || !Enemy->GetHealthComponent() || Enemy->GetHealthComponent()->IsDead())
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Origin, Enemy->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Enemy;
			}
		}

		return Best;
	}
}

void USlimeCheatManager::SlimeDumpTables()
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStatTableProvider* Provider = GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;

	if (!Provider)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeDumpTables: no UStatTableProvider available."));
		return;
	}

	Provider->DumpLoadedTables();
}

void USlimeCheatManager::SlimeReloadTables()
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStatTableProvider* Provider = GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;

	if (!Provider)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeReloadTables: no UStatTableProvider available."));
		return;
	}

	const bool bOk = Provider->ReloadTables();
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeReloadTables: %s"), bOk ? TEXT("ok") : TEXT("failed, see the log above"));
}

void USlimeCheatManager::SlimeDamageNearestEnemy(float Amount)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;

	ASlimeEnemyBase* BestEnemy = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		ASlimeEnemyBase* Enemy = *It;
		if (!Enemy || !Enemy->GetHealthComponent() || Enemy->GetHealthComponent()->IsDead())
		{
			continue;
		}

		const float DistSq = PlayerPawn
			? FVector::DistSquared(PlayerPawn->GetActorLocation(), Enemy->GetActorLocation())
			: 0.f;

		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestEnemy = Enemy;
		}
	}

	if (!BestEnemy)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeDamageNearestEnemy: no live enemy found in the world."));
		return;
	}

	USlimeCombatSubsystem* CombatSubsystem = USlimeCombatSubsystem::Get(this);
	if (!CombatSubsystem)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeDamageNearestEnemy: USlimeCombatSubsystem is unavailable."));
		return;
	}

	CombatSubsystem->ApplyDamageTo(BestEnemy, Amount, PlayerPawn);
}

void USlimeCheatManager::SlimeDumpBodyMetrics()
{
	DumpBodyMetricsNow();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BodyMetricsTimer,
			this,
			&USlimeCheatManager::RetryDumpBodyMetrics,
			0.5f,
			false);

		World->GetTimerManager().SetTimer(
			BodyMetricsLateTimer,
			this,
			&USlimeCheatManager::RetryDumpBodyMetricsLate,
			2.5f,
			false);
	}
}

void USlimeCheatManager::RetryDumpBodyMetrics()
{
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeDumpBodyMetrics: retrying after deferred spawn initialisation."));
	DumpBodyMetricsNow();
}

void USlimeCheatManager::RetryDumpBodyMetricsLate()
{
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeDumpBodyMetrics: late retry after fusion/approach time."));
	DumpBodyMetricsNow();
}

void USlimeCheatManager::DumpBodyMetricsNow()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		const ASlimeEnemyBase* Enemy = *It;
		if (!Enemy)
		{
			continue;
		}

		++Count;

		const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		const UStaticMeshComponent* BodyMesh = Enemy->GetBodyMeshComponent();
		if (!Capsule)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("BodyMetrics: %s mass=%d has no capsule."),
				*GetNameSafe(Enemy),
				Enemy->GetMass());
			continue;
		}

		if (!BodyMesh || !BodyMesh->GetStaticMesh())
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("BodyMetrics: %s mass=%d radius=%.1f halfHeight=%.1f has no visual mesh."),
				*GetNameSafe(Enemy),
				Enemy->GetMass(),
				Capsule->GetScaledCapsuleRadius(),
				Capsule->GetScaledCapsuleHalfHeight());
			continue;
		}

		const FVector ActorLocation = Enemy->GetActorLocation();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float CapsuleBottom = ActorLocation.Z - HalfHeight;

		const FBox MeshBox = BodyMesh->Bounds.GetBox();
		const float MeshBottom = MeshBox.Min.Z;
		const float MeshCentreOffsetXY =
			FVector::Dist2D(MeshBox.GetCenter(), ActorLocation);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeBodyMetrics), false);
		QueryParams.AddIgnoredActor(Enemy);

		FHitResult GroundHit;
		const bool bFoundGround = World->LineTraceSingleByChannel(
			GroundHit,
			ActorLocation + FVector(0.f, 0.f, 100.f),
			ActorLocation - FVector(0.f, 0.f, 1000.f),
			ECC_WorldStatic,
			QueryParams);

		if (bFoundGround)
		{
			const float GroundZ = GroundHit.ImpactPoint.Z;
			UE_LOG(LogSlimeWar, Log,
				TEXT("BodyMetrics: %s mass=%d radius=%.1f halfHeight=%.1f scale=%.3f ")
				TEXT("capsuleBottomErr=%.1f meshBottomErr=%.1f meshCentreXY=%.1f groundZ=%.1f"),
				*GetNameSafe(Enemy),
				Enemy->GetMass(),
				Capsule->GetScaledCapsuleRadius(),
				HalfHeight,
				BodyMesh->GetComponentScale().X,
				CapsuleBottom - GroundZ,
				MeshBottom - GroundZ,
				MeshCentreOffsetXY,
				GroundZ);
		}
		else
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("BodyMetrics: %s mass=%d has no WorldStatic ground below it ")
				TEXT("(capsuleBottom=%.1f meshBottom=%.1f)."),
				*GetNameSafe(Enemy),
				Enemy->GetMass(),
				CapsuleBottom,
				MeshBottom);
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeDumpBodyMetrics: checked %d live slime(s)."), Count);
}

void USlimeCheatManager::RefreshEcologyTestList()
{
	EcologyTestSlimes.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimeEcologyEnemy> It(World); It; ++It)
	{
		if (ASlimeEcologyEnemy* Enemy = *It)
		{
			EcologyTestSlimes.Add(Enemy);
		}
	}
}

FVector USlimeCheatManager::ComputeEcologyGroupCentre() const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			Sum += Enemy->GetActorLocation();
			++Count;
		}
	}

	return Count > 0 ? Sum / static_cast<float>(Count) : FVector::ZeroVector;
}

float USlimeCheatManager::ComputeMinimumPairDistance() const
{
	float Minimum = TNumericLimits<float>::Max();
	for (int32 A = 0; A < EcologyTestSlimes.Num(); ++A)
	{
		const ASlimeEcologyEnemy* First = EcologyTestSlimes[A].Get();
		if (!First)
		{
			continue;
		}

		for (int32 B = A + 1; B < EcologyTestSlimes.Num(); ++B)
		{
			const ASlimeEcologyEnemy* Second = EcologyTestSlimes[B].Get();
			if (Second)
			{
				Minimum = FMath::Min(
					Minimum,
					FVector::Dist2D(First->GetActorLocation(), Second->GetActorLocation()));
			}
		}
	}

	return Minimum < TNumericLimits<float>::Max() ? Minimum : -1.f;
}

float USlimeCheatManager::ComputeAverageGroupDistance() const
{
	const FVector Centre = ComputeEcologyGroupCentre();
	float Sum = 0.f;
	int32 Count = 0;

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			Sum += FVector::Dist2D(Enemy->GetActorLocation(), Centre);
			++Count;
		}
	}

	return Count > 0 ? Sum / static_cast<float>(Count) : -1.f;
}

int32 USlimeCheatManager::CountStaticPenetrations() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Penetrations = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const UCapsuleComponent* Capsule = Enemy ? Enemy->GetCapsuleComponent() : nullptr;
		if (!Enemy || !Capsule)
		{
			continue;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeEcologyPenetration), false, Enemy);
		if (World->OverlapBlockingTestByChannel(
			Enemy->GetActorLocation(),
			FQuat::Identity,
			ECC_WorldStatic,
			FCollisionShape::MakeSphere(Capsule->GetScaledCapsuleRadius() * 0.85f),
			QueryParams))
		{
			++Penetrations;
		}
	}

	return Penetrations;
}

void USlimeCheatManager::SlimeSpawnEcology(int32 Count, int32 Faction, int32 Mass)
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !PlayerPawn)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeSpawnEcology: needs a running world and player pawn."));
		return;
	}

	SlimeClearEnemies();
	EcologyTestSlimes.Reset();

	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal2D();
	EcologyTestCentre = PlayerPawn->GetActorLocation() + Forward * 650.f;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ESlimeFaction EcologyFaction = static_cast<ESlimeFaction>(
		FMath::Clamp(Faction, 0, static_cast<int32>(ESlimeFaction::C)));
	const int32 SpawnCount = FMath::Max(1, Count);
	const int32 SpawnMass = FMath::Clamp(Mass, 1, 8);

	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(SpawnCount);
		const float RingRadius = 260.f + 70.f * (Index % 2);
		FVector SpawnLocation = EcologyTestCentre + FVector(
			FMath::Cos(Angle) * RingRadius,
			FMath::Sin(Angle) * RingRadius,
			0.f);

		FNavLocation Projected;
		if (NavSystem && NavSystem->ProjectPointToNavigation(
			SpawnLocation,
			Projected,
			FVector(200.f, 200.f, 400.f)))
		{
			SpawnLocation = Projected.Location;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeEcologySpawn), false, PlayerPawn);
		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(
			GroundHit,
			SpawnLocation + FVector(0.f, 0.f, 100.f),
			SpawnLocation - FVector(0.f, 0.f, 1000.f),
			ECC_WorldStatic,
			QueryParams))
		{
			SpawnLocation.Z = GroundHit.ImpactPoint.Z + 90.f;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASlimeEcologyEnemy* Enemy = World->SpawnActor<ASlimeEcologyEnemy>(
			ASlimeEcologyEnemy::StaticClass(),
			SpawnLocation,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (!Enemy)
		{
			continue;
		}

		Enemy->ApplyStatRow(SpawnMass);
		Enemy->InitializeEcology(EcologyFaction, 0, EcologyTestCentre, 1200.f);
		EcologyTestSlimes.Add(Enemy);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeSpawnEcology: spawned %d ecology slime(s) around %s."),
		EcologyTestSlimes.Num(),
		*EcologyTestCentre.ToCompactString());
}

void USlimeCheatManager::SlimeEcologyStatus()
{
	RefreshEcologyTestList();

	const USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	const FVector Centre = ComputeEcologyGroupCentre();

	int32 MaxCurrentStuck = 0;
	int32 StuckOverTwoSeconds = 0;
	int32 TotalPathFailures = 0;
	int32 TotalPathRequests = 0;
	int32 MaxRefreshCount = 0;

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const USlimeEcologyBrainComponent* Brain = Enemy ? Enemy->GetEcologyBrain() : nullptr;
		const USlimeLocomotionComponent* Locomotion = Enemy ? Enemy->GetLocomotion() : nullptr;
		if (!Enemy || !Brain || !Locomotion)
		{
			continue;
		}

		const float CurrentStuck = Locomotion->GetCurrentStuckTime();
		MaxCurrentStuck = FMath::Max(MaxCurrentStuck, FMath::FloorToInt(CurrentStuck * 100.f));
		StuckOverTwoSeconds += CurrentStuck >= 2.f ? 1 : 0;
		TotalPathFailures += Locomotion->GetPathFailureCount();
		TotalPathRequests += Locomotion->GetPathRequestCount();
		MaxRefreshCount = FMath::Max(MaxRefreshCount, Brain->GetRefreshCount());

		UE_LOG(LogSlimeWar, Log,
			TEXT("Ecology: %s behaviour=%s goal=%s speed=%.0f allies=%d pathPoints=%d ")
			TEXT("pathFailures=%d stuck=%.2f atGoal=%s playerDistance=%.1f playerVisible=%s ")
			TEXT("velocity=%.1f maxWalk=%.1f"),
			*GetNameSafe(Enemy),
			Brain->GetBehaviorName(),
			*Brain->GetDesiredGoal().ToCompactString(),
			Brain->GetDesiredSpeed(),
			Brain->GetVisibleAllyCount(),
			Locomotion->GetPathPointCount(),
			Locomotion->GetPathFailureCount(),
			CurrentStuck,
			Locomotion->IsAtGoal() ? TEXT("yes") : TEXT("no"),
			Brain->GetPlayerDistance(),
			Brain->IsPlayerVisible() ? TEXT("yes") : TEXT("no"),
			Enemy->GetVelocity().Size2D(),
			Enemy->GetCharacterMovement()->MaxWalkSpeed);
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const float ExpectedGap = Rules
		? 2.f * 20.f + Rules->ExtraSpacing
		: 65.f;

	UE_LOG(LogSlimeWar, Log,
		TEXT("EcologySummary: live=%d registered=%d rebuilds=%d visitedCells=%d centre=%s ")
		TEXT("avgGroupDistance=%.1f minPairDistance=%.1f expectedGap=%.1f ")
		TEXT("penetrations=%d stuck>2s=%d maxStuck=%.2f pathFailures=%d pathRequests=%d refreshMax=%d"),
		EcologyTestSlimes.Num(),
		Ecology ? Ecology->GetRegisteredCount() : -1,
		Ecology ? Ecology->GetRebuildCount() : -1,
		Ecology ? Ecology->GetLastQueryVisitedCells() : -1,
		*Centre.ToCompactString(),
		ComputeAverageGroupDistance(),
		ComputeMinimumPairDistance(),
		ExpectedGap,
		CountStaticPenetrations(),
		StuckOverTwoSeconds,
		static_cast<float>(MaxCurrentStuck) / 100.f,
		TotalPathFailures,
		TotalPathRequests,
		MaxRefreshCount);
}

ASlimeEcologyEnemy* USlimeCheatManager::FindNearestEcologyEnemy() const
{
	UWorld* World = GetWorld();
	const APlayerController* PlayerController = GetPlayerController();
	const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	const FVector Origin = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	ASlimeEcologyEnemy* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASlimeEcologyEnemy> It(World); It; ++It)
	{
		ASlimeEcologyEnemy* Enemy = *It;
		if (!Enemy || Enemy->IsActorBeingDestroyed())
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Origin, Enemy->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Enemy;
		}
	}

	return Best;
}

void USlimeCheatManager::SlimeEcologyBodyStatus()
{
	RefreshEcologyTestList();
	auto StateName = [](ESlimeBodyState State)
	{
		switch (State)
		{
		case ESlimeBodyState::Normal:		return TEXT("Normal");
		case ESlimeBodyState::AimLocked:	return TEXT("AimLocked");
		case ESlimeBodyState::StunPending:	return TEXT("StunPending");
		case ESlimeBodyState::Stunned:		return TEXT("Stunned");
		default:							return TEXT("Unknown");
		}
	};

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const USlimeBodyInteractionComponent* Body =
			Enemy ? Enemy->GetBodyInteraction() : nullptr;
		if (!Body)
		{
			continue;
		}

		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologyBody: %s state=%s external=%.1f observed=%.1f pending=%.2f ")
			TEXT("stun=%.2f settle=%.2f bodyPushWindow=%.2f position=%s"),
			*GetNameSafe(Enemy),
			StateName(Body->GetBodyState()),
			Body->GetExternalVelocity().Size2D(),
			Body->GetObservedSpeed(),
			Body->GetStunPending(),
			Body->GetStunRemaining(),
			Body->GetSettleTime(),
			Body->GetForcedFusionWindowRemaining(),
			*Enemy->GetActorLocation().ToCompactString());
	}
}

void USlimeCheatManager::SlimeEcologyAimLock(int32 Locked)
{
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeBodyInteractionComponent* Body = Enemy ? Enemy->GetBodyInteraction() : nullptr;
	if (!Body)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyAimLock: no ecology slime found."));
		return;
	}

	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	Body->SetAimLocked(Locked != 0, PlayerPawn);
	if (USlimeEcologyBrainComponent* Brain = Enemy->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyAimLock: %s aimLocked=%s."),
		*GetNameSafe(Enemy),
		Body->IsAimLocked() ? TEXT("yes") : TEXT("no"));
}

void USlimeCheatManager::SlimeEcologyExternalPush(float Speed)
{
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeBodyInteractionComponent* Body = Enemy ? Enemy->GetBodyInteraction() : nullptr;
	if (!Body)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyExternalPush: no ecology slime found."));
		return;
	}

	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	FVector Direction = PlayerPawn
		? Enemy->GetActorLocation() - PlayerPawn->GetActorLocation()
		: Enemy->GetActorForwardVector();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal2D();
	Body->ApplyExternalVelocity(Direction * FMath::Max(0.f, Speed), PlayerPawn);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyExternalPush: applied %.1f cm/s to %s."),
		Speed,
		*GetNameSafe(Enemy));
}

void USlimeCheatManager::SlimeEcologyStunTest(float Seconds)
{
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeBodyInteractionComponent* Body = Enemy ? Enemy->GetBodyInteraction() : nullptr;
	if (!Body)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyStunTest: no ecology slime found."));
		return;
	}

	Body->ReceiveStun(Seconds);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyStunTest: queued %.2f s stun on %s."),
		Seconds,
		*GetNameSafe(Enemy));
}

void USlimeCheatManager::SlimeEcologyBodyTest()
{
	SlimeSpawnEcology(3, static_cast<int32>(ESlimeFaction::A), 1);
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeBodyInteractionComponent* Body = Enemy ? Enemy->GetBodyInteraction() : nullptr;
	if (!Body)
	{
		return;
	}

	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	FVector Direction = PlayerPawn
		? Enemy->GetActorLocation() - PlayerPawn->GetActorLocation()
		: Enemy->GetActorForwardVector();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal2D();

	EcologyBodyTestTarget = Enemy;
	EcologyBodyTestStage = 0;
	Body->ApplyExternalVelocity(Direction * 400.f, PlayerPawn);
	Body->ReceiveStun(1.5f);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyBodyTest: applied 400 cm/s and pending 1.5 s stun to %s."),
		*GetNameSafe(Enemy));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyBodyTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyBodyTest,
			0.4f,
			false);
	}
}

void USlimeCheatManager::AdvanceEcologyBodyTest()
{
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyBodyTest: stage %d."),
		EcologyBodyTestStage);
	SlimeEcologyBodyStatus();

	++EcologyBodyTestStage;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (EcologyBodyTestStage == 1)
	{
		World->GetTimerManager().SetTimer(
			EcologyBodyTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyBodyTest,
			0.8f,
			false);
	}
	else if (EcologyBodyTestStage == 2)
	{
		World->GetTimerManager().SetTimer(
			EcologyBodyTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyBodyTest,
			2.f,
			false);
	}
	else if (EcologyBodyTestStage == 3)
	{
		if (ASlimeEcologyEnemy* Target = EcologyBodyTestTarget.Get())
		{
			EcologyAimLockAnchor = Target->GetActorLocation();
			if (USlimeBodyInteractionComponent* Body = Target->GetBodyInteraction())
			{
				APawn* PlayerPawn =
					GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
				Body->SetAimLocked(true, PlayerPawn);
			}
		}

		World->GetTimerManager().SetTimer(
			EcologyBodyTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyBodyTest,
			1.5f,
			false);
	}
	else if (EcologyBodyTestStage == 4)
	{
		if (ASlimeEcologyEnemy* Target = EcologyBodyTestTarget.Get())
		{
			UE_LOG(LogSlimeWar, Log,
				TEXT("SlimeEcologyBodyTest: aim-lock movement delta=%.1f cm."),
				FVector::Dist2D(EcologyAimLockAnchor, Target->GetActorLocation()));
			if (USlimeBodyInteractionComponent* Body = Target->GetBodyInteraction())
			{
				Body->SetAimLocked(false);
			}
		}

		if (APlayerController* PlayerController = GetPlayerController())
		{
			PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
		}
	}
	else if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeEcologySoakTest(float Seconds)
{
	RefreshEcologyTestList();
	if (EcologyTestSlimes.Num() < 2)
	{
		SlimeSpawnEcology(10, static_cast<int32>(ESlimeFaction::A));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	EcologySoakStartTime = World->GetTimeSeconds();
	EcologyPathRequestsAtStart = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (const USlimeLocomotionComponent* Locomotion = Enemy->GetLocomotion())
			{
				EcologyPathRequestsAtStart += Locomotion->GetPathRequestCount();
			}
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologySoakTest: started %.1f second soak with %d slime(s)."),
		FMath::Max(1.f, Seconds),
		EcologyTestSlimes.Num());

	World->GetTimerManager().SetTimer(
		EcologySoakTimer,
		this,
		&USlimeCheatManager::FinishEcologySoak,
		FMath::Max(1.f, Seconds),
		false);
}

void USlimeCheatManager::FinishEcologySoak()
{
	RefreshEcologyTestList();
	SlimeEcologyStatus();

	UWorld* World = GetWorld();
	int32 PathRequests = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (const USlimeLocomotionComponent* Locomotion = Enemy->GetLocomotion())
			{
				PathRequests += Locomotion->GetPathRequestCount();
			}
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologySoakTest: finished after %.1f s; path requests during soak=%d."),
		World ? World->GetTimeSeconds() - EcologySoakStartTime : -1.f,
		PathRequests - EcologyPathRequestsAtStart);

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeEcologyAvoidanceTest()
{
	RefreshEcologyTestList();
	if (EcologyTestSlimes.Num() < 2)
	{
		SlimeSpawnEcology(10, static_cast<int32>(ESlimeFaction::A));
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyAvoidanceTest: allowing the group to settle for 2 s before player contact."));
	World->GetTimerManager().SetTimer(
		EcologyAvoidanceTimer,
		this,
		&USlimeCheatManager::BeginEcologyAvoidanceContact,
		2.f,
		false);
}

void USlimeCheatManager::BeginEcologyAvoidanceContact()
{
	RefreshEcologyTestList();

	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !PlayerPawn)
	{
		return;
	}

	const FVector Centre = ComputeEcologyGroupCentre();
	EcologyPrimaryAvoidanceSubject = nullptr;
	float FarthestDistance = -1.f;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		if (!Enemy)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Centre);
		if (Distance > FarthestDistance)
		{
			FarthestDistance = Distance;
			EcologyPrimaryAvoidanceSubject = Enemy;
		}
	}

	FVector Target = Centre + FVector(180.f, 0.f, 0.f);
	ASlimeEcologyEnemy* PrimarySubject = EcologyPrimaryAvoidanceSubject.Get();
	if (PrimarySubject)
	{
		FVector Outward = PrimarySubject->GetActorLocation() - Centre;
		Outward.Z = 0.f;
		Outward = Outward.GetSafeNormal2D();

		FVector SubjectLocation = Centre + Outward * 900.f;
		FNavLocation SubjectProjected;
		UNavigationSystemV1* SubjectNav =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (SubjectNav && SubjectNav->ProjectPointToNavigation(
			SubjectLocation,
			SubjectProjected,
			FVector(250.f, 250.f, 500.f)))
		{
			SubjectLocation = SubjectProjected.Location;
			SubjectLocation.Z = PrimarySubject->GetActorLocation().Z;
		}

		PrimarySubject->SetActorLocation(
			SubjectLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (USlimeLocomotionComponent* SubjectLocomotion = PrimarySubject->GetLocomotion())
		{
			SubjectLocomotion->ClearMoveGoal();
		}
		if (USlimeEcologyBrainComponent* SubjectBrain = PrimarySubject->GetEcologyBrain())
		{
			SubjectBrain->ForceRefresh();
		}

		Target = SubjectLocation - Outward * 180.f;
	}

	FNavLocation Projected;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSystem && NavSystem->ProjectPointToNavigation(
		Target,
		Projected,
		FVector(200.f, 200.f, 400.f)))
	{
		Target = Projected.Location;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeEcologyAvoidance), false, PlayerPawn);
	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
		GroundHit,
		Target + FVector(0.f, 0.f, 100.f),
		Target - FVector(0.f, 0.f, 1000.f),
		ECC_WorldStatic,
		QueryParams))
	{
		Target.Z = GroundHit.ImpactPoint.Z + 90.f;
	}

	PlayerPawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	EcologyAvoidanceSubjects.Reset();

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (USlimeEcologyBrainComponent* Brain = Enemy->GetEcologyBrain())
			{
				Brain->ForceRefresh();
			}
		}
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyAvoidanceTest: player placed %.0f cm from group centre, ")
		TEXT("%.0f cm from primary subject %s; enter radius %.0f cm; waiting 0.6 s for perception."),
		FVector::Dist2D(PlayerPawn->GetActorLocation(), Centre),
		PrimarySubject
			? FVector::Dist2D(PlayerPawn->GetActorLocation(), PrimarySubject->GetActorLocation())
			: -1.f,
		*GetNameSafe(PrimarySubject),
		Rules ? Rules->PlayerAvoidEnterDistance : 240.f);

	World->GetTimerManager().SetTimer(
		EcologyAvoidanceTimer,
		this,
		&USlimeCheatManager::ConfirmEcologyAvoidanceSubjects,
		0.6f,
		false);
}

void USlimeCheatManager::ConfirmEcologyAvoidanceSubjects()
{
	RefreshEcologyTestList();
	EcologyAvoidanceSubjects.Reset();

	int32 Avoiders = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const USlimeEcologyBrainComponent* Brain = Enemy ? Enemy->GetEcologyBrain() : nullptr;
		if (Brain && Brain->GetBehavior() == ESlimeEcologyBehavior::AvoidPlayer)
		{
			++Avoiders;
		}
	}

	if (ASlimeEcologyEnemy* PrimarySubject = EcologyPrimaryAvoidanceSubject.Get())
	{
		const USlimeEcologyBrainComponent* Brain = PrimarySubject->GetEcologyBrain();
		if (Brain && Brain->GetBehavior() == ESlimeEcologyBehavior::AvoidPlayer)
		{
			EcologyAvoidanceSubjects.Add(PrimarySubject);
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyAvoidanceTest: %d slime(s) entered AvoidPlayer; primary subject active=%s; ")
		TEXT("measuring the 4 s exit window."),
		Avoiders,
		EcologyAvoidanceSubjects.Num() > 0 ? TEXT("yes") : TEXT("no"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyAvoidanceTimer,
			this,
			&USlimeCheatManager::BeginEcologyAvoidanceRecovery,
			4.f,
			false);
	}
}

void USlimeCheatManager::BeginEcologyAvoidanceRecovery()
{
	RefreshEcologyTestList();
	SlimeEcologyStatus();

	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!PlayerPawn || !World)
	{
		return;
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const float ExitDistance = Rules ? Rules->PlayerAvoidExitDistance : 320.f;
	float MinSubjectDistance = TNumericLimits<float>::Max();
	int32 SubjectsOutsideExit = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyAvoidanceSubjects)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			const float Distance =
				FVector::Dist2D(Enemy->GetActorLocation(), PlayerPawn->GetActorLocation());
			MinSubjectDistance = FMath::Min(MinSubjectDistance, Distance);
			SubjectsOutsideExit += Distance > ExitDistance ? 1 : 0;
		}
	}
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyAvoidanceTest: after 4 s %d / %d avoidance subject(s) are outside %.0f cm; ")
		TEXT("minimum subject distance=%.1f cm."),
		SubjectsOutsideExit,
		EcologyAvoidanceSubjects.Num(),
		ExitDistance,
		MinSubjectDistance < TNumericLimits<float>::Max() ? MinSubjectDistance : -1.f);

	const FVector Centre = ComputeEcologyGroupCentre();
	FVector Away = PlayerPawn->GetActorLocation() - Centre;
	Away.Z = 0.f;
	Away = Away.GetSafeNormal2D();
	FVector Target = Centre + Away * 700.f;

	FNavLocation Projected;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSystem && NavSystem->ProjectPointToNavigation(
		Target,
		Projected,
		FVector(200.f, 200.f, 400.f)))
	{
		Target = Projected.Location;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeEcologyRecovery), false, PlayerPawn);
	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
		GroundHit,
		Target + FVector(0.f, 0.f, 100.f),
		Target - FVector(0.f, 0.f, 1000.f),
		ECC_WorldStatic,
		QueryParams))
	{
		Target.Z = GroundHit.ImpactPoint.Z + 90.f;
	}

	PlayerPawn->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (USlimeEcologyBrainComponent* Brain = Enemy->GetEcologyBrain())
			{
				Brain->ForceRefresh();
			}
		}
	}

	World->GetTimerManager().SetTimer(
		EcologyAvoidanceTimer,
		this,
		&USlimeCheatManager::FinishEcologyAvoidance,
		10.f,
		false);
}

void USlimeCheatManager::FinishEcologyAvoidance()
{
	RefreshEcologyTestList();
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyAvoidanceTest: recovery window finished."));
	SlimeEcologyStatus();

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeGroundingTest()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!World || !PlayerPawn)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeGroundingTest: needs a running world and player pawn."));
		return;
	}

	SlimeClearEnemies();
	GroundingTestSlimes.Reset();
	GroundingTestStage = 0;

	const ASlimeEnemyBase* CDO = ASlimeNormal::StaticClass()->GetDefaultObject<ASlimeEnemyBase>();
	const UCapsuleComponent* CDOCapsule = CDO ? CDO->GetCapsuleComponent() : nullptr;
	const float SpawnHalfHeight =
		CDOCapsule ? CDOCapsule->GetUnscaledCapsuleHalfHeight() : 90.f;

	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward.Normalize();

	const FVector SpawnCentre = PlayerPawn->GetActorLocation() + Forward * 450.f;
	const FVector Right = PlayerPawn->GetActorRightVector();
	const int32 TestMasses[] = {1, 4, 8};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeGroundingTest), false);
	QueryParams.AddIgnoredActor(PlayerPawn);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TestMasses); ++Index)
	{
		FVector SpawnLocation = SpawnCentre + Right * ((Index - 1) * 300.f);

		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(
			GroundHit,
			SpawnLocation + FVector(0.f, 0.f, 100.f),
			SpawnLocation - FVector(0.f, 0.f, 1000.f),
			ECC_WorldStatic,
			QueryParams))
		{
			SpawnLocation.Z = GroundHit.ImpactPoint.Z + SpawnHalfHeight + 2.f;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASlimeEnemyBase* Slime = World->SpawnActor<ASlimeEnemyBase>(
			ASlimeNormal::StaticClass(),
			SpawnLocation,
			PlayerPawn->GetActorRotation(),
			SpawnParameters);

		if (!Slime)
		{
			continue;
		}

		Slime->InitializeFromSpawn(0, SpawnLocation);
		Slime->ApplyStatRow(TestMasses[Index]);
		GroundingTestSlimes.Add(Slime);

		UE_LOG(LogSlimeWar, Log,
			TEXT("SlimeGroundingTest: spawned %s at mass %d, location %s."),
			*GetNameSafe(Slime),
			TestMasses[Index],
			*SpawnLocation.ToCompactString());
	}

	World->GetTimerManager().SetTimer(
		GroundingTestTimer,
		this,
		&USlimeCheatManager::FinishGroundingTest,
		0.75f,
		false);
}

void USlimeCheatManager::FinishGroundingTest()
{
	int32 ValidCount = 0;
	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakSlime : GroundingTestSlimes)
	{
		ValidCount += WeakSlime.IsValid() ? 1 : 0;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeGroundingTest: stage %d has %d / %d valid slime(s)."),
		GroundingTestStage,
		ValidCount,
		GroundingTestSlimes.Num());

	DumpBodyMetricsNow();

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}

	if (GroundingTestStage != 0)
	{
		return;
	}

	GroundingTestStage = 1;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GroundingMassChangeTimer,
			this,
			&USlimeCheatManager::ApplyGroundingRuntimeMassChange,
			0.35f,
			false);

		World->GetTimerManager().SetTimer(
			GroundingTestTimer,
			this,
			&USlimeCheatManager::FinishGroundingTest,
			1.f,
			false);
	}
}

void USlimeCheatManager::ApplyGroundingRuntimeMassChange()
{
	ASlimeEnemyBase* FirstSlime =
		GroundingTestSlimes.IsValidIndex(0) ? GroundingTestSlimes[0].Get() : nullptr;
	if (!FirstSlime)
	{
		return;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeGroundingTest: changing %s from mass %d to mass 8 at runtime."),
		*GetNameSafe(FirstSlime),
		FirstSlime->GetMass());
	FirstSlime->ApplyStatRow(8);
}

void USlimeCheatManager::SlimeSpawnNormal(int32 Count)
{
	SpawnSlimes(GetWorld(), GetPlayerController(), ASlimeNormal::StaticClass(), Count);
}

void USlimeCheatManager::SlimeSpawnAggro(int32 Count)
{
	SpawnSlimes(GetWorld(), GetPlayerController(), ASlimeAggro::StaticClass(), Count);
}

void USlimeCheatManager::SlimeClearEnemies()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Destroyed = 0;
	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		if (ASlimeEnemyBase* Enemy = *It)
		{
			Enemy->Destroy();
			++Destroyed;
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeClearEnemies: destroyed %d enemies."), Destroyed);
}

void USlimeCheatManager::SlimeKillPlayer()
{
	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	USlimeCombatSubsystem* CombatSubsystem = USlimeCombatSubsystem::Get(this);

	if (!PlayerPawn || !CombatSubsystem)
	{
		return;
	}

	// Deal exactly the player's maximum health so the whole damage path is exercised.
	const USlimeHealthComponent* Health = PlayerPawn->FindComponentByClass<USlimeHealthComponent>();
	if (!Health)
	{
		return;
	}

	CombatSubsystem->ApplyDamageTo(PlayerPawn, Health->GetMaxHealth(), PlayerPawn);
}

void USlimeCheatManager::SlimeSpawnNormalAtMass(int32 Mass, int32 Count)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Tight cluster so the CP-2 fusion scenes (two fusing, three touching, damaged fusion)
	// reproduce in a couple of seconds instead of waiting for the wander AI to converge.
	// The gap follows the body size: a fixed 120 cm placed big slimes inside each other.
	const int32 SpawnMass = FMath::Max(1, Mass);
	float Spacing = SlimeCheatFusionSpacing;

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (const UStatTableProvider* Provider = GameInstance->GetSubsystem<UStatTableProvider>())
		{
			FSlimeStatRow Row;
			if (Provider->GetSlimeStat(SpawnMass, Row) && Row.BodyRadius > 0.f)
			{
				Spacing = FMath::Max(Spacing, Row.BodyRadius * 2.f + 20.f);
			}
		}
	}

	const TArray<ASlimeEnemyBase*> Spawned =
		SpawnSlimes(World, GetPlayerController(), ASlimeNormal::StaticClass(), Count, Spacing);

	for (ASlimeEnemyBase* Slime : Spawned)
	{
		if (Slime)
		{
			Slime->ApplyStatRow(SpawnMass);
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeSpawnNormalAtMass: spawned %d normal slime(s) at mass %d."),
		Spawned.Num(), FMath::Max(1, Mass));
}

void USlimeCheatManager::SlimeSetMass(int32 Mass)
{
	ASlimeEnemyBase* Slime = FindNearestNormalSlime(GetWorld(), GetPlayerController());
	if (!Slime)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeSetMass: no live normal slime found."));
		return;
	}

	const int32 NewMass = FMath::Max(1, Mass);
	Slime->ApplyStatRow(NewMass);
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeSetMass: %s is now mass %d."), *GetNameSafe(Slime), NewMass);
}

void USlimeCheatManager::SlimeForceFuse()
{
	UWorld* World = GetWorld();
	ASlimeEnemyBase* First = FindNearestNormalSlime(World, GetPlayerController());
	if (!World || !First)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeForceFuse: no live normal slime found."));
		return;
	}

	// The nearest other normal slime of the same spawn point (fusion never crosses points).
	ASlimeEnemyBase* Second = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		ASlimeEnemyBase* Candidate = *It;
		if (!Candidate || Candidate == First || Candidate->IsAggressive())
		{
			continue;
		}

		if (Candidate->GetPointId() != First->GetPointId())
		{
			continue;
		}

		if (!Candidate->GetHealthComponent() || Candidate->GetHealthComponent()->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(First->GetActorLocation(), Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Second = Candidate;
		}
	}

	if (!Second)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeForceFuse: need at least two normal slimes at the same point."));
		return;
	}

	USlimeFusionComponent* Fusion = First->GetFusionComponent();
	if (!Fusion || !Fusion->DebugForcePair(Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeForceFuse: could not pair %s with %s."),
			*GetNameSafe(First), *GetNameSafe(Second));
		return;
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeForceFuse: forced %s (mass %d) + %s (mass %d)."),
		*GetNameSafe(First), First->GetMass(), *GetNameSafe(Second), Second->GetMass());
}

void USlimeCheatManager::SlimeRunStart()
{
	URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRunStart: no URunSubsystem in this world."));
		return;
	}

	Run->StartRun();
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeRunStart: run state is now %s."),
		SlimeFlowNames::RunState(Run->GetRunState()));
}

void USlimeCheatManager::SlimeRunEnd()
{
	URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRunEnd: no URunSubsystem in this world."));
		return;
	}

	Run->EndRun(ERunEndReason::TimeUp);
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeRunEnd: run state is now %s."),
		SlimeFlowNames::RunState(Run->GetRunState()));
}

void USlimeCheatManager::SlimeRunTimeScale(float Scale)
{
	SlimeCVars::RunTimeScale = FMath::Max(0.f, Scale);

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeRunTimeScale: the run timeline now advances at %.2fx (0 freezes it)."),
		SlimeCVars::RunTimeScale);
}

void USlimeCheatManager::SlimeRunStatus()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	URunSubsystem* Run = URunSubsystem::Get(this);
	UScoreSubsystem* Score = UScoreSubsystem::Get(this);
	const USlimeEnemyManagerSubsystem* Manager = USlimeEnemyManagerSubsystem::Get(this);

	if (!Run || !Score)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRunStatus: the run / score subsystem is unavailable."));
		return;
	}

	UE_LOG(LogSlimeWar, Log, TEXT("=== SlimeWar run status (CP-3) ==="));
	UE_LOG(LogSlimeWar, Log, TEXT("state %s   elapsed %.1f s   remaining %d s   batch %d / %d"),
		SlimeFlowNames::RunState(Run->GetRunState()),
		Run->GetElapsedTime(),
		Run->GetRemainingSeconds(),
		Run->GetLastIssuedBatch() == INDEX_NONE ? 0 : Run->GetLastIssuedBatch() + 1,
		Run->GetBatchCount());

	// -- score, plus the per-mass breakdown the hand check compares against --
	TArray<int32> KillsByMass;
	TArray<int32> ScoreByMass;
	Score->GetKillBreakdown(KillsByMass, ScoreByMass);

	int32 BreakdownScore = 0;
	for (int32 Mass = 1; Mass < KillsByMass.Num(); ++Mass)
	{
		BreakdownScore += ScoreByMass.IsValidIndex(Mass) ? ScoreByMass[Mass] : 0;

		if (KillsByMass[Mass] > 0)
		{
			UE_LOG(LogSlimeWar, Log, TEXT("  mass %d: %d kill(s) -> %d points"),
				Mass, KillsByMass[Mass], ScoreByMass.IsValidIndex(Mass) ? ScoreByMass[Mass] : 0);
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("score %d (per-mass sum %d)   target %d   best %d   locked %s"),
		Score->GetCurrentScore(),
		BreakdownScore,
		Score->GetTargetScore(),
		Score->GetBestScore(),
		Score->IsScoringLocked() ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogSlimeWar, Log, TEXT("normal kills %d   cleared points %d   fusions %d"),
		Score->GetNormalKillCount(), Score->GetClearedPointCount(), Run->GetEnemyFusedCount());

	if (Manager)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("enemies live %d (normal %d / aggro %d)   peak %d"),
			Manager->GetLiveEnemyCount(),
			Manager->GetLiveCount(ETargetKind::Normal),
			Manager->GetLiveCount(ETargetKind::Aggressive),
			Manager->GetPeakLiveEnemyCount());
	}

	// -- per point: the numbers behind the four point states --
	for (TActorIterator<ASpawnPoint> It(World); It; ++It)
	{
		const ASpawnPoint* Point = *It;
		if (!Point)
		{
			continue;
		}

		UE_LOG(LogSlimeWar, Log,
			TEXT("point %d: %s   spawned normal %d / %d   live normal %d   live aggro %d   pending %d   supply done %s"),
			Point->GetPointId(),
			SlimeFlowNames::PointState(Point->GetState()),
			Point->GetSpawnedNormalCount(), Point->GetTotalNormalSupply(),
			Point->GetLiveNormalCount(), Point->GetLiveAggroCount(),
			Point->GetPendingSpawnCount(),
			Point->IsSupplyDone() ? TEXT("yes") : TEXT("no"));
	}

	UE_LOG(LogSlimeWar, Log, TEXT("=== end of run status ==="));
}

void USlimeCheatManager::SlimeRunDeploy(int32 DropPointIndex)
{
	URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRunDeploy: the run subsystem is unavailable."));
		return;
	}

	if (!Run->BeginDeployment(DropPointIndex))
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("SlimeRunDeploy: could not deploy (phase %s, index %d)."),
			SlimeFlowNames::RunState(Run->GetRunState()), DropPointIndex);
	}
}

void USlimeCheatManager::SlimeRunResult()
{
	const URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRunResult: the run subsystem is unavailable."));
		return;
	}

	const FSlimeRunResult Result = Run->GetRunResult();

	UE_LOG(LogSlimeWar, Log, TEXT("=== SlimeWar settlement (CP-4) ==="));
	UE_LOG(LogSlimeWar, Log, TEXT("phase %s   end reason %s   drop point %d"),
		SlimeFlowNames::RunState(Run->GetRunState()),
		SlimeFlowNames::EndReason(Result.EndReason),
		Run->GetSelectedDropPoint());
	UE_LOG(LogSlimeWar, Log, TEXT("score %d / target %d   best %d   passed %s"),
		Result.Score, Result.TargetScore, Result.BestScore,
		Result.bPassed ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogSlimeWar, Log, TEXT("normal kills %d   cleared points %d"),
		Result.NormalKills, Result.ClearedPoints);
	UE_LOG(LogSlimeWar, Log, TEXT("=== end of settlement ==="));
}

void USlimeCheatManager::SlimeRetry()
{
	URunSubsystem* Run = URunSubsystem::Get(this);
	if (!Run)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeRetry: the run subsystem is unavailable."));
		return;
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeRetry: reloading the level (drop point %d)."), Run->GetSelectedDropPoint());
	Run->RequestRetry(/*bReselectDropPoint=*/false);
}

void USlimeCheatManager::SlimeRunSetTime(float SecondsRemaining)
{
	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->DebugSetRemainingSeconds(SecondsRemaining);
	}
}

void USlimeCheatManager::SlimeRunAddScore(int32 Points)
{
	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->DebugAddScore(Points);
	}
}

void USlimeCheatManager::SlimePause()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->TogglePause();
		return;
	}

	UE_LOG(LogSlimeWar, Error, TEXT("SlimePause: the UI subsystem is unavailable."));
}
