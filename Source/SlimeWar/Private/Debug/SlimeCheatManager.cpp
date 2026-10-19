// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeCheatManager.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeAlertComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyGrenade.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeFactionSite.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Ecology/SlimePaintSubsystem.h"
#include "Ecology/SlimePaintField.h"
#include "Ecology/SlimeWeakpointComponent.h"
#include "Enemy/SlimeAggro.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeFusionComponent.h"
#include "Enemy/SlimeNormal.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/RunSubsystem.h"
#include "Flow/ScoreSubsystem.h"
#include "Flow/SlimeEnemyManagerSubsystem.h"
#include "Flow/SlimeFlowMath.h"
#include "Flow/SlimeFlowNames.h"
#include "Flow/SpawnPoint.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Math/NumericLimits.h"
#include "NavigationSystem.h"
#include "Player/SlimeAimAssistComponent.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"
#include "Player/SlimeWeaponRules.h"
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
		const float RingRadius = SpawnCount == 1
			? 0.f
			: 260.f + 70.f * (Index % 2);
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

void USlimeCheatManager::SlimeEcologyAlertTest()
{
        SlimeSpawnEcology(4, static_cast<int32>(ESlimeFaction::A), 1);
        if (EcologyTestSlimes.Num() < 3)
        {
                return;
        }

        ASlimeEcologyEnemy* Victim = EcologyTestSlimes[0].Get();
        ASlimeEcologyEnemy* Attacker = EcologyTestSlimes[1].Get();
        if (!Victim || !Attacker)
        {
                return;
        }
        Attacker->SetFaction(ESlimeFaction::B);

        TWeakObjectPtr<ASlimeEcologyEnemy> WeakVictim = Victim;
        TWeakObjectPtr<ASlimeEcologyEnemy> WeakAttacker = Attacker;
        if (UWorld* World = GetWorld())
        {
                World->GetTimerManager().SetTimer(
                        EcologyAlertTestTimer,
                        FTimerDelegate::CreateLambda([this, WeakVictim, WeakAttacker]()
                        {
                                ASlimeEcologyEnemy* TestVictim = WeakVictim.Get();
                                ASlimeEcologyEnemy* TestAttacker = WeakAttacker.Get();
                                if (!TestVictim || !TestAttacker)
                                {
                                        return;
                                }

                                if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this))
                                {
                                        Combat->ApplyDamageTo(TestVictim, 1.f, TestAttacker);
                                }

                                int32 AlertedAllies = 0;
                                for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
                                {
                                        const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
                                        const USlimeAlertComponent* Alert =
                                                Enemy ? Enemy->GetAlertComponent() : nullptr;
                                        if (Enemy && Enemy != TestAttacker
                                                && Enemy->GetFaction() == TestVictim->GetFaction()
                                                && Alert && Alert->IsAlerted())
                                        {
                                                ++AlertedAllies;
                                        }
                                }

                                const USlimeAlertComponent* VictimAlert = TestVictim->GetAlertComponent();
                                const USlimeAlertComponent* AttackerAlert = TestAttacker->GetAlertComponent();
                                UE_LOG(LogSlimeWar, Log,
                                        TEXT("SlimeEcologyAlertTest: victimAlerted=%s warning=%.2f alert=%.2f ")
                                        TEXT("alliesAlerted=%d attackerAlerted=%s."),
                                        VictimAlert && VictimAlert->IsAlerted() ? TEXT("yes") : TEXT("no"),
                                        VictimAlert ? VictimAlert->GetWarningRemaining() : 0.f,
                                        VictimAlert ? VictimAlert->GetAlertRemaining() : 0.f,
                                        AlertedAllies,
                                        AttackerAlert && AttackerAlert->IsAlerted() ? TEXT("yes") : TEXT("no"));
                        }),
                        0.25f,
                        false);
        }
}

void USlimeCheatManager::SlimeEcologyDisputeTest()
{
        SlimeSpawnEcology(2, static_cast<int32>(ESlimeFaction::A), 1);
        if (EcologyTestSlimes.Num() < 2)
        {
                return;
        }

        ASlimeEcologyEnemy* First = EcologyTestSlimes[0].Get();
        ASlimeEcologyEnemy* Second = EcologyTestSlimes[1].Get();
        if (!First || !Second)
        {
                return;
        }
        Second->SetFaction(ESlimeFaction::B);
        if (USlimeAlertComponent* Alert = First->GetAlertComponent())
        {
                Alert->SetDisputeCondition(true, 1.6f);
                UE_LOG(LogSlimeWar, Log,
                        TEXT("SlimeEcologyDisputeTest: alerted=%s warning=%.2f cooldown=%.2f."),
                        Alert->IsAlerted() ? TEXT("yes") : TEXT("no"),
                        Alert->GetWarningRemaining(),
                        Alert->GetDisputeCooldownRemaining());
        }
}

void USlimeCheatManager::SlimeEcologyHostileFusionTest()
{
        ASlimeEcologyEnemy* First = nullptr;
        ASlimeEcologyEnemy* Second = nullptr;
        if (!SpawnEcologyPairForTest(1, 40.f, First, Second))
        {
                return;
        }

        if (USlimeAlertComponent* Alert = First->GetAlertComponent())
        {
                Alert->RaiseAlert(Second, false);
        }
        USlimeEcologyFusionComponent* Fusion = First->GetEcologyFusionComponent();
        const bool bFused = Fusion && Fusion->DebugForcedPair(Second);
        const USlimeCoreComponent* Core = First->GetCoreComponent();
        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeEcologyHostileFusionTest: fused=%s mass=%d core=%d hostile=%s."),
                bFused ? TEXT("yes") : TEXT("no"),
                First->GetMass(),
                Core ? static_cast<int32>(Core->GetCoreState()) : -1,
                Core && Core->IsHostileCore() ? TEXT("yes") : TEXT("no"));
}

void USlimeCheatManager::SlimeWeaponDoesNotAlertTest()
{
        SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 1);
        ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
        APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
        if (!Enemy || !PlayerPawn)
        {
                return;
        }

        if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this))
        {
                Combat->ApplyDamageTo(Enemy, 1.f, PlayerPawn);
        }
        const USlimeAlertComponent* Alert = Enemy->GetAlertComponent();
        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeWeaponDoesNotAlertTest: alerted=%s (expected no)."),
                Alert && Alert->IsAlerted() ? TEXT("yes") : TEXT("no"));
}

void USlimeCheatManager::SlimeGrenadeStatus()
{
        const ASlimeWarCharacter* Player =
                GetPlayerController() ? Cast<ASlimeWarCharacter>(GetPlayerController()->GetPawn()) : nullptr;
        if (!Player)
        {
                return;
        }

        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeGrenadeStatus: selected=%s faction=%d assimilation=%d tornado=%d."),
                ASlimeEcologyGrenade::TypeName(Player->GetSelectedGrenadeType()),
                static_cast<int32>(Player->GetSelectedGrenadeFaction()),
                Player->GetAssimilationGrenadeCount(),
                Player->GetTornadoGrenadeCount());
}

void USlimeCheatManager::SlimeThrowGrenade(int32 Type)
{
        UWorld* World = GetWorld();
        APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
        if (!World || !PlayerPawn)
        {
                return;
        }

        const ESlimeGrenadeType GrenadeType = Type == 1
                ? ESlimeGrenadeType::Tornado
                : ESlimeGrenadeType::Assimilation;
        const FVector Direction = PlayerPawn->GetActorForwardVector();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASlimeEcologyGrenade* Grenade = World->SpawnActor<ASlimeEcologyGrenade>(
                ASlimeEcologyGrenade::StaticClass(),
                PlayerPawn->GetActorLocation() + Direction * 100.f + FVector(0.f, 0.f, 80.f),
                FRotator::ZeroRotator,
                Params);
        if (Grenade)
        {
                Grenade->Launch(GrenadeType, ESlimeFaction::B, Direction);
                UE_LOG(LogSlimeWar, Log,
                        TEXT("SlimeThrowGrenade: spawned %s."),
                        ASlimeEcologyGrenade::TypeName(GrenadeType));
        }
}

void USlimeCheatManager::SlimeGrenadeAssimilationTest()
{
        SlimeSpawnEcology(3, static_cast<int32>(ESlimeFaction::A), 1);
        if (EcologyTestSlimes.Num() > 0)
        {
                if (ASlimeEcologyEnemy* Carrier = EcologyTestSlimes[0].Get())
                {
                        Carrier->SetCargo(1);
                }
        }
        UWorld* World = GetWorld();
        if (!World)
        {
                return;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASlimeEcologyGrenade* Grenade = World->SpawnActor<ASlimeEcologyGrenade>(
                ASlimeEcologyGrenade::StaticClass(),
                EcologyTestCentre + FVector(0.f, 0.f, 300.f),
                FRotator::ZeroRotator,
                Params);
        if (Grenade)
        {
                Grenade->Launch(ESlimeGrenadeType::Assimilation, ESlimeFaction::B, FVector::DownVector);
        }
        EcologyGrenadeTestStage = 0;
        World->GetTimerManager().SetTimer(
                EcologyGrenadeTestTimer,
                this,
                &USlimeCheatManager::AdvanceEcologyGrenadeTest,
                1.2f,
                false);
}

void USlimeCheatManager::SlimeGrenadeTornadoTest()
{
        SlimeSpawnEcology(3, static_cast<int32>(ESlimeFaction::A), 1);
        UWorld* World = GetWorld();
        if (!World)
        {
                return;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASlimeEcologyGrenade* Grenade = World->SpawnActor<ASlimeEcologyGrenade>(
                ASlimeEcologyGrenade::StaticClass(),
                EcologyTestCentre + FVector(0.f, 0.f, 300.f),
                FRotator::ZeroRotator,
                Params);
        if (Grenade)
        {
                Grenade->Launch(ESlimeGrenadeType::Tornado, ESlimeFaction::A, FVector::DownVector);
        }
        EcologyGrenadeTestStage = 10;
        World->GetTimerManager().SetTimer(
                EcologyGrenadeTestTimer,
                this,
                &USlimeCheatManager::AdvanceEcologyGrenadeTest,
                1.2f,
                false);
}

void USlimeCheatManager::SlimeGrenadeTornadoWallTest()
{
        ASlimeEcologyEnemy* Near = nullptr;
        ASlimeEcologyEnemy* Far = nullptr;
        if (!SpawnEcologyPairForTest(1, 500.f, Near, Far))
        {
                UE_LOG(LogSlimeWar, Warning, TEXT("SlimeGrenadeTornadoWallTest: pair spawn failed."));
                return;
        }

        UWorld* World = GetWorld();
        if (!World)
        {
                return;
        }

        const FVector Midpoint = (Near->GetActorLocation() + Far->GetActorLocation()) * 0.5f;
        const FRotator WallRotation =
                (Far->GetActorLocation() - Near->GetActorLocation()).Rotation();
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(
                AStaticMeshActor::StaticClass(),
                Midpoint - FVector(0.f, 0.f, 120.f),
                WallRotation,
                Params);
        if (!Wall)
        {
                return;
        }

        if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
        {
                UStaticMeshComponent* WallMesh = Wall->GetStaticMeshComponent();
                WallMesh->SetMobility(EComponentMobility::Movable);
                WallMesh->SetStaticMesh(Cube);
                WallMesh->SetCollisionObjectType(ECC_WorldStatic);
                WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
                WallMesh->SetMobility(EComponentMobility::Static);
        }
        Wall->SetActorScale3D(FVector(0.35f, 3.5f, 5.f));

        FActorSpawnParameters GrenadeParams;
        GrenadeParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASlimeEcologyGrenade* Grenade = World->SpawnActor<ASlimeEcologyGrenade>(
                ASlimeEcologyGrenade::StaticClass(),
                Near->GetActorLocation() + FVector(0.f, 0.f, 250.f),
                FRotator::ZeroRotator,
                GrenadeParams);
        if (Grenade)
        {
                Grenade->Launch(ESlimeGrenadeType::Tornado, ESlimeFaction::A, FVector::DownVector);
        }

        EcologyTornadoNear = Near;
        EcologyTornadoFar = Far;
        EcologyTornadoWall = Wall;
        EcologyGrenadeTestStage = 20;
        World->GetTimerManager().SetTimer(
                EcologyGrenadeTestTimer,
                this,
                &USlimeCheatManager::AdvanceEcologyGrenadeTest,
                1.2f,
                false);
}

void USlimeCheatManager::AdvanceEcologyGrenadeTest()
{
        RefreshEcologyTestList();
        int32 FactionBCount = 0;
        int32 AssignedSiteBCount = 0;
        float MaxExternalSpeed = 0.f;
        for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
        {
                const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
                if (!Enemy)
                {
                        continue;
                }
                FactionBCount += Enemy->GetFaction() == ESlimeFaction::B ? 1 : 0;
                AssignedSiteBCount += Enemy->GetAssignedSite()
                        && Enemy->GetAssignedSite()->Faction == ESlimeFaction::B ? 1 : 0;
                if (const USlimeBodyInteractionComponent* Body = Enemy->GetBodyInteraction())
                {
                        MaxExternalSpeed =
                                FMath::Max(MaxExternalSpeed, Body->GetExternalVelocity().Size2D());
                }
        }

        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeGrenadeTest: stage=%d factionB=%d assignedB=%d maxExternalSpeed=%.1f."),
                EcologyGrenadeTestStage,
                FactionBCount,
                AssignedSiteBCount,
                MaxExternalSpeed);
        SlimePaintStatus();
        if (EcologyGrenadeTestStage == 20)
        {
                const ASlimeEcologyEnemy* Near = EcologyTornadoNear.Get();
                const ASlimeEcologyEnemy* Far = EcologyTornadoFar.Get();
                const USlimeBodyInteractionComponent* NearBody =
                        Near ? Near->GetBodyInteraction() : nullptr;
                const USlimeBodyInteractionComponent* FarBody =
                        Far ? Far->GetBodyInteraction() : nullptr;
                UE_LOG(LogSlimeWar, Log,
                        TEXT("SlimeGrenadeTornadoWallTest: nearExternal=%.1f farExternal=%.1f ")
                        TEXT("(far must be 0 through the wall)."),
                        NearBody ? NearBody->GetExternalVelocity().Size2D() : -1.f,
                        FarBody ? FarBody->GetExternalVelocity().Size2D() : -1.f);
                if (AActor* Wall = EcologyTornadoWall.Get())
                {
                        Wall->Destroy();
                }
                EcologyTornadoWall.Reset();
                if (APlayerController* PlayerController = GetPlayerController())
                {
                        PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
                }
                return;
        }
        if (EcologyGrenadeTestStage == 0)
        {
                SlimeEcologyResourceStatus();
        }

        if (EcologyGrenadeTestStage == 0)
        {
                EcologyGrenadeTestStage = 1;
                if (UWorld* World = GetWorld())
                {
                        World->GetTimerManager().SetTimer(
                                EcologyGrenadeTestTimer,
                                this,
                                &USlimeCheatManager::AdvanceEcologyGrenadeTest,
                                2.f,
                                false);
                }
                return;
        }

        if (EcologyGrenadeTestStage == 1)
        {
                for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
                {
                        const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
                        const ASlimeFactionSite* Site = Enemy ? Enemy->GetAssignedSite() : nullptr;
                        if (Enemy && Enemy->GetCargo() > 0)
                        {
                                UE_LOG(LogSlimeWar, Log,
                                        TEXT("SlimeGrenadeTest: carrier=%s faction=%d site=%s ")
                                        TEXT("brain=%s goal=%s cargo=%d."),
                                        *GetNameSafe(Enemy),
                                        static_cast<int32>(Enemy->GetFaction()),
                                        Site ? *Site->SiteId.ToString() : TEXT("None"),
                                        Enemy->GetEcologyBrain()
                                                ? Enemy->GetEcologyBrain()->GetBehaviorName()
                                                : TEXT("None"),
                                        Enemy->GetEcologyBrain()
                                                ? *Enemy->GetEcologyBrain()->GetDesiredGoal().ToCompactString()
                                                : TEXT("None"),
                                        Enemy->GetCargo());
                        }
                }
        }

        if (EcologyGrenadeTestStage == 10)
        {
                ++EcologyGrenadeTestStage;
                if (UWorld* World = GetWorld())
                {
                        World->GetTimerManager().SetTimer(
                                EcologyGrenadeTestTimer,
                                this,
                                &USlimeCheatManager::AdvanceEcologyGrenadeTest,
                                3.8f,
                                false);
                }
                return;
        }

        if (APlayerController* PlayerController = GetPlayerController())
        {
                PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
        }
}

void USlimeCheatManager::SlimeEcologyScoreTest()
{
        FSlimeKillContext Normal;
        Normal.Mass = 8;
        Normal.BodyScoreCredit = 68;
        Normal.bKilledByPlayer = true;

        FSlimeKillContext WithCargo = Normal;
        WithCargo.Cargo = 2;

        FSlimeKillContext Hostile = WithCargo;
        Hostile.bHostileCore = true;
        Hostile.CoreState = ESlimeCoreState::Hostile;

        FSlimeKillContext Npc = Normal;
        Npc.bKilledByPlayer = false;

        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeEcologyScoreTest: normal=%d cargo2=%d hostile=%d npc=%d (expect 68/80/40/0)."),
                SlimeFlowMath::EcologyKillScore(Normal),
                SlimeFlowMath::EcologyKillScore(WithCargo),
                SlimeFlowMath::EcologyKillScore(Hostile),
                SlimeFlowMath::EcologyKillScore(Npc));
}

void USlimeCheatManager::SlimePaintStatus()
{
        USlimePaintSubsystem* Paint = GetWorld()
                ? GetWorld()->GetSubsystem<USlimePaintSubsystem>()
                : nullptr;
        if (!Paint)
        {
                return;
        }

        const UTextureRenderTarget2D* RT = Paint->GetPaintRenderTarget();
        int32 PaintFields = 0;
        if (UWorld* World = GetWorld())
        {
                for (TActorIterator<ASlimePaintField> It(World); It; ++It)
                {
                        ++PaintFields;
                }
        }
        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimePaintStatus: splats=%d records=%d rt=%s size=%dx%d fields=%d debug=%d."),
                Paint->GetSplatCount(),
                Paint->GetSplatRecords().Num(),
                RT ? TEXT("valid") : TEXT("none"),
                RT ? RT->SizeX : 0,
                RT ? RT->SizeY : 0,
                PaintFields,
                SlimeCVars::DebugPaint);
}

void USlimeCheatManager::SlimePaintTest()
{
        USlimePaintSubsystem* Paint = GetWorld()
                ? GetWorld()->GetSubsystem<USlimePaintSubsystem>()
                : nullptr;
        APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
        if (!Paint || !PlayerPawn)
        {
                return;
        }

        Paint->BeginRun();
        const FVector Centre =
                PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * 500.f;
        for (int32 Index = 0; Index < 6; ++Index)
        {
                const FVector Offset(
                        (Index - 2) * 80.f,
                        (Index % 2 == 0 ? -1.f : 1.f) * 70.f,
                        2.f);
                Paint->RegisterSplat(
                        Centre + Offset,
                        Index % 2 == 0
                                ? Paint->GetFactionColor(ESlimeFaction::A)
                                : Paint->GetFactionColor(ESlimeFaction::B),
                        55.f + Index * 6.f,
                        Index * 0.4f);
        }
        SlimePaintStatus();
        if (APlayerController* PlayerController = GetPlayerController())
        {
                PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
        }
}

bool USlimeCheatManager::SpawnEcologyPairForTest(
	int32 Mass,
	float Spacing,
	ASlimeEcologyEnemy*& OutFirst,
	ASlimeEcologyEnemy*& OutSecond)
{
	OutFirst = nullptr;
	OutSecond = nullptr;
	SlimeSpawnEcology(2, static_cast<int32>(ESlimeFaction::A), Mass);
	if (EcologyTestSlimes.Num() < 2)
	{
		return false;
	}

	OutFirst = EcologyTestSlimes[0].Get();
	OutSecond = EcologyTestSlimes[1].Get();
	if (!OutFirst || !OutSecond)
	{
		return false;
	}

	FVector Direction = OutFirst->GetActorForwardVector();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}
	const FVector SecondLocation =
		OutFirst->GetActorLocation() + Direction.GetSafeNormal2D() * Spacing;
	OutSecond->SetActorLocation(SecondLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (USlimeLocomotionComponent* Loco = OutFirst->GetLocomotion())
	{
		Loco->ClearMoveGoal();
	}
	if (USlimeLocomotionComponent* Loco = OutSecond->GetLocomotion())
	{
		Loco->ClearMoveGoal();
	}
	if (USlimeEcologyBrainComponent* Brain = OutFirst->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}
	if (USlimeEcologyBrainComponent* Brain = OutSecond->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}
	if (USlimeEcologyFusionComponent* Fusion = OutFirst->GetEcologyFusionComponent())
	{
		Fusion->ForceReady();
	}
	if (USlimeEcologyFusionComponent* Fusion = OutSecond->GetEcologyFusionComponent())
	{
		Fusion->ForceReady();
	}
	return true;
}

void USlimeCheatManager::SlimeEcologyFusionStatus()
{
	RefreshEcologyTestList();
	auto FusionStateName = [](ESlimeEcologyFusionState InState)
	{
		switch (InState)
		{
		case ESlimeEcologyFusionState::Idle:			return TEXT("Idle");
		case ESlimeEcologyFusionState::Seeking:		return TEXT("Seeking");
		case ESlimeEcologyFusionState::Contacting:	return TEXT("Contacting");
		case ESlimeEcologyFusionState::Cooldown:		return TEXT("Cooldown");
		case ESlimeEcologyFusionState::Blocked:		return TEXT("Blocked");
		default:									return TEXT("Unknown");
		}
	};
	auto FusionPathName = [](ESlimeEcologyFusionPath InPath)
	{
		switch (InPath)
		{
		case ESlimeEcologyFusionPath::None:			return TEXT("None");
		case ESlimeEcologyFusionPath::Autonomous:	return TEXT("Autonomous");
		case ESlimeEcologyFusionPath::Contact:		return TEXT("Contact");
		case ESlimeEcologyFusionPath::Forced:		return TEXT("Forced");
		default:									return TEXT("Unknown");
		}
	};

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const USlimeEcologyFusionComponent* Fusion =
			Enemy ? Enemy->GetEcologyFusionComponent() : nullptr;
		if (!Fusion)
		{
			continue;
		}

		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologyFusion: %s mass=%d state=%s path=%s partner=%s contact=%.2f ")
			TEXT("ready=%.2f cooldown=%.2f block=%.2f cargo=%d credit=%d health=%.1f/%.1f ")
			TEXT("position=%s velocity=%.1f brain=%s locoGoal=%s pathPoints=%d ")
			TEXT("pathFailures=%d atGoal=%s"),
			*GetNameSafe(Enemy),
			Enemy->GetMass(),
			FusionStateName(Fusion->GetState()),
			FusionPathName(Fusion->GetPath()),
			*GetNameSafe(Fusion->GetPartner()),
			Fusion->GetContactAlpha(),
			Fusion->GetAutoReadyRemaining(),
			Fusion->GetCooldownRemaining(),
			Fusion->GetBlockRemaining(),
			Enemy->GetCargo(),
			Enemy->GetBodyScoreCredit(),
			Enemy->GetHealthComponent() ? Enemy->GetHealthComponent()->GetHealth() : 0.f,
			Enemy->GetHealthComponent() ? Enemy->GetHealthComponent()->GetMaxHealth() : 0.f,
			*Enemy->GetActorLocation().ToCompactString(),
			Enemy->GetVelocity().Size2D(),
			Enemy->GetEcologyBrain() ? Enemy->GetEcologyBrain()->GetBehaviorName() : TEXT("None"),
			Enemy->GetLocomotion() ? *Enemy->GetLocomotion()->GetGoal().ToCompactString() : TEXT("None"),
			Enemy->GetLocomotion() ? Enemy->GetLocomotion()->GetPathPointCount() : -1,
			Enemy->GetLocomotion() ? Enemy->GetLocomotion()->GetPathFailureCount() : -1,
			Enemy->GetLocomotion() && Enemy->GetLocomotion()->IsAtGoal() ? TEXT("yes") : TEXT("no"));
	}
}

void USlimeCheatManager::SlimeEcologyFusionReady()
{
	RefreshEcologyTestList();
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent())
			{
				Fusion->ForceReady();
			}
		}
	}
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyFusionReady: all ecology slimes are ready."));
}

void USlimeCheatManager::SlimeEcologyForceFuse()
{
	RefreshEcologyTestList();
	for (int32 A = 0; A < EcologyTestSlimes.Num(); ++A)
	{
		ASlimeEcologyEnemy* First = EcologyTestSlimes[A].Get();
		if (!First)
		{
			continue;
		}
		for (int32 B = A + 1; B < EcologyTestSlimes.Num(); ++B)
		{
			ASlimeEcologyEnemy* Second = EcologyTestSlimes[B].Get();
			if (Second && First->GetFaction() == Second->GetFaction())
			{
				USlimeEcologyFusionComponent* Fusion = First->GetEcologyFusionComponent();
				if (Fusion && Fusion->DebugForcedPair(Second))
				{
					UE_LOG(LogSlimeWar, Log,
						TEXT("SlimeEcologyForceFuse: forced %s + %s."),
						*GetNameSafe(First),
						*GetNameSafe(Second));
					return;
				}
			}
		}
	}
	UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyForceFuse: no valid pair."));
}

void USlimeCheatManager::SlimeEcologyInterruptFusion()
{
	RefreshEcologyTestList();
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent())
			{
				if (Fusion->IsEngaged())
				{
					Fusion->InterruptFusion();
					UE_LOG(LogSlimeWar, Log,
						TEXT("SlimeEcologyInterruptFusion: interrupted fusion on %s."),
						*GetNameSafe(Enemy));
					return;
				}
			}
		}
	}
	UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyInterruptFusion: no engaged fusion."));
}

void USlimeCheatManager::SlimeEcologyInterruptTest()
{
	ASlimeEcologyEnemy* First = nullptr;
	ASlimeEcologyEnemy* Second = nullptr;
	if (!SpawnEcologyPairForTest(1, 50.f, First, Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyInterruptTest: pair spawn failed."));
		return;
	}

	EcologyFusionTestPrimary = First;
	EcologyFusionTestSecondary = Second;
	EcologyFusionTestStage = 0;
	bEcologyFusionForcedTest = false;
	bEcologyFusionAutoTest = false;
	bEcologyFusionInterruptTest = true;
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyInterruptTest: pair spawned; interrupting after contact starts."));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyFusionTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyFusionTest,
			0.6f,
			false);
	}
}

void USlimeCheatManager::SlimeEcologyPushPairSetup()
{
	ASlimeEcologyEnemy* First = nullptr;
	ASlimeEcologyEnemy* Second = nullptr;
	if (!SpawnEcologyPairForTest(1, 180.f, First, Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyPushPairSetup: pair spawn failed."));
		return;
	}

	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	for (ASlimeEcologyEnemy* Enemy : {First, Second})
	{
		if (USlimeBodyInteractionComponent* Body = Enemy->GetBodyInteraction())
		{
			Body->SetAimLocked(true, PlayerPawn);
		}
		if (USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent())
		{
			Fusion->InterruptFusion(30.f);
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyPushPairSetup: two mass 1 slimes are 180 cm apart and stationary. ")
		TEXT("Walk into one and push it into the other."));
}

void USlimeCheatManager::SlimeEcologyFusionTest()
{
	ASlimeEcologyEnemy* First = nullptr;
	ASlimeEcologyEnemy* Second = nullptr;
	if (!SpawnEcologyPairForTest(1, 50.f, First, Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyFusionTest: pair spawn failed."));
		return;
	}

	EcologyFusionTestPrimary = First;
	EcologyFusionTestSecondary = Second;
	EcologyFusionTestStage = 0;
	bEcologyFusionForcedTest = false;
	bEcologyFusionAutoTest = false;
	bEcologyFusionInterruptTest = false;
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyFusionTest: spawned mass 1 pair at 50 cm."));
	SlimeEcologyFusionStatus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyFusionTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyFusionTest,
			1.2f,
			false);
	}
}

void USlimeCheatManager::SlimeEcologyAutoFusionTest()
{
	ASlimeEcologyEnemy* First = nullptr;
	ASlimeEcologyEnemy* Second = nullptr;
	if (!SpawnEcologyPairForTest(1, 250.f, First, Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyAutoFusionTest: pair spawn failed."));
		return;
	}

	EcologyFusionTestPrimary = First;
	EcologyFusionTestSecondary = Second;
	EcologyFusionTestStage = 0;
	bEcologyFusionForcedTest = false;
	bEcologyFusionAutoTest = true;
	bEcologyFusionInterruptTest = false;
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyAutoFusionTest: spawned ready mass 1 pair at 250 cm."));
	SlimeEcologyFusionStatus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyFusionTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyFusionTest,
			2.f,
			false);
	}
}

void USlimeCheatManager::SlimeEcologyForcedFusionTest()
{
	ASlimeEcologyEnemy* First = nullptr;
	ASlimeEcologyEnemy* Second = nullptr;
	if (!SpawnEcologyPairForTest(4, 120.f, First, Second))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyForcedFusionTest: pair spawn failed."));
		return;
	}

	EcologyFusionTestPrimary = First;
	EcologyFusionTestSecondary = Second;
	EcologyFusionTestStage = 0;
	bEcologyFusionForcedTest = true;
	bEcologyFusionAutoTest = false;
	bEcologyFusionInterruptTest = false;

	FVector Direction = Second->GetActorLocation() - First->GetActorLocation();
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal2D();
	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	if (USlimeBodyInteractionComponent* Body = First->GetBodyInteraction())
	{
		Body->ApplyExternalVelocity(Direction * 500.f, PlayerPawn);
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyForcedFusionTest: spawned mass 4 pair and pushed one at 500 cm/s."));
	SlimeEcologyFusionStatus();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyFusionTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyFusionTest,
			1.f,
			false);
	}
}

void USlimeCheatManager::AdvanceEcologyFusionTest()
{
	if (bEcologyFusionInterruptTest)
	{
		SlimeEcologyFusionStatus();
		if (EcologyFusionTestStage == 0)
		{
			if (ASlimeEcologyEnemy* First = EcologyFusionTestPrimary.Get())
			{
				if (USlimeEcologyFusionComponent* Fusion = First->GetEcologyFusionComponent())
				{
					Fusion->InterruptFusion();
				}
			}
			UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyInterruptTest: fusion interrupted."));
			EcologyFusionTestStage = 1;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					EcologyFusionTestTimer,
					this,
					&USlimeCheatManager::AdvanceEcologyFusionTest,
					0.4f,
					false);
			}
			return;
		}

		if (EcologyFusionTestStage == 1)
		{
			ASlimeEcologyEnemy* First = EcologyFusionTestPrimary.Get();
			ASlimeEcologyEnemy* Second = EcologyFusionTestSecondary.Get();
			if (First && Second)
			{
				FVector Direction = Second->GetActorLocation() - First->GetActorLocation();
				Direction.Z = 0.f;
				Direction = Direction.GetSafeNormal2D();
				APawn* PlayerPawn =
					GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
				if (USlimeBodyInteractionComponent* FirstBody = First->GetBodyInteraction())
				{
					FirstBody->SetAimLocked(true, PlayerPawn);
				}
				if (USlimeBodyInteractionComponent* SecondBody = Second->GetBodyInteraction())
				{
					SecondBody->SetAimLocked(true, PlayerPawn);
				}
				if (USlimeBodyInteractionComponent* Body = First->GetBodyInteraction())
				{
					Body->ApplyExternalVelocity(Direction * 800.f, PlayerPawn);
				}
			}
			UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyInterruptTest: applied forced-fusion velocity during block."));
			EcologyFusionTestStage = 2;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					EcologyFusionTestTimer,
					this,
					&USlimeCheatManager::AdvanceEcologyFusionTest,
					1.f,
					false);
			}
			return;
		}

		SlimeEcologyFusionStatus();
		DumpBodyMetricsNow();
		if (APlayerController* PlayerController = GetPlayerController())
		{
			PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
		}
		return;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyFusionTest: %s stage %d."),
		bEcologyFusionForcedTest
			? TEXT("forced")
			: (bEcologyFusionAutoTest ? TEXT("autonomous") : TEXT("contact")),
		EcologyFusionTestStage);
	SlimeEcologyFusionStatus();

	++EcologyFusionTestStage;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (EcologyFusionTestStage < 4)
	{
		const float NextDelay = bEcologyFusionAutoTest ? 2.f : 1.2f;
		World->GetTimerManager().SetTimer(
			EcologyFusionTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyFusionTest,
			NextDelay,
			false);
		return;
	}

	DumpBodyMetricsNow();
	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeSpawnEcologyCore(int32 Mass, int32 CoreState)
{
	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), FMath::Clamp(Mass, 1, 8));
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
	if (!Core)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeSpawnEcologyCore: no ecology slime found."));
		return;
	}

	Core->ArmCore(static_cast<ESlimeCoreState>(FMath::Clamp(CoreState, 0, 2)));
	EcologyCoreTestCore = Enemy;
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeSpawnEcologyCore: %s mass %d coreState %d."),
		*GetNameSafe(Enemy),
		Enemy->GetMass(),
		CoreState);
}

void USlimeCheatManager::SlimeEcologySetCoreState(int32 CoreState)
{
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
	if (!Core)
	{
		return;
	}

	Core->ArmCore(static_cast<ESlimeCoreState>(FMath::Clamp(CoreState, 0, 2)));
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologySetCoreState: %s -> %d."),
		*GetNameSafe(Enemy),
		CoreState);
}

void USlimeCheatManager::SlimeEcologyProvokePlayer(float Seconds)
{
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
	if (!Core)
	{
		return;
	}

	Core->SetPlayerProvoked(Seconds);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyProvokePlayer: %s provoked for %.1f s."),
		*GetNameSafe(Enemy),
		Seconds);
}

void USlimeCheatManager::SlimeEcologyCoreStatus()
{
	RefreshEcologyTestList();
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get();
		const USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
		const USlimeWeakpointComponent* Weakpoint = Enemy ? Enemy->GetWeakpointComponent() : nullptr;
		const USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
		if (!Enemy || !Core)
		{
			continue;
		}

		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologyCore: %s state=%d phase=%s mass=%d hp=%.1f/%.1f target=%s ")
			TEXT("wake=%.2f windup=%.2f pounce=%.2f cd=%.2f provoked=%.2f weakpoint=%s exploded=%s"),
			*GetNameSafe(Enemy),
			static_cast<int32>(Core->GetCoreState()),
			Core->GetPhaseName(),
			Enemy->GetMass(),
			Health ? Health->GetHealth() : 0.f,
			Health ? Health->GetMaxHealth() : 0.f,
			Core->GetCombatTarget() ? *GetNameSafe(Core->GetCombatTarget()) : TEXT("None"),
			Core->GetWakeRemaining(),
			Core->GetWindupRemaining(),
			Core->GetPounceRemaining(),
			Core->GetAttackCooldownRemaining(),
			Core->GetPlayerProvokedRemaining(),
			Weakpoint && Weakpoint->IsWeakpointActive() ? TEXT("yes") : TEXT("no"),
			Core->HasExploded() ? TEXT("yes") : TEXT("no"));
	}
}

void USlimeCheatManager::SlimeEcologyPounceTest()
{
	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 2);
	UWorld* World = GetWorld();
	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
	if (!World || !PlayerPawn || !Enemy || !Core)
	{
		return;
	}

	const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
	FVector SpawnLocation = PlayerPawn->GetActorLocation() + Forward * 300.f;
	SpawnLocation.Z = Enemy->GetActorLocation().Z;
	Enemy->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetFaction(ESlimeFaction::A);
	Core->ArmCore(ESlimeCoreState::Hostile);
	Core->SetPlayerProvoked(10.f);
	Core->ForceTargetForTest(PlayerPawn);
	EcologyCoreTestCore = Enemy;
	EcologyCoreTestStage = 0;

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyPounceTest: core placed 300 cm ahead, hostile and targeting the player."));
	SlimeEcologyCoreStatus();

	World->GetTimerManager().SetTimer(
		EcologyCoreTestTimer,
		this,
		&USlimeCheatManager::AdvanceEcologyCoreTest,
		0.4f,
		false);
}

void USlimeCheatManager::SlimeEcologyPounceLockTest()
{
        SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 2);
        UWorld* World = GetWorld();
        APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
        ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
        USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
        if (!World || !PlayerPawn || !Enemy || !Core)
        {
                return;
        }

        const FVector Forward = PlayerPawn->GetActorForwardVector().GetSafeNormal2D();
        FVector SpawnLocation = PlayerPawn->GetActorLocation() + Forward * 300.f;
        SpawnLocation.Z = Enemy->GetActorLocation().Z;
        Enemy->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
        Core->ArmCore(ESlimeCoreState::Hostile);
        Core->SetPlayerProvoked(10.f);
        Core->ForceTargetForTest(PlayerPawn);
        EcologyCoreTestCore = Enemy;
        EcologyPounceLockStage = 0;

        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeEcologyPounceLockTest: core placed at 300 cm; locking after wind-up."));
        World->GetTimerManager().SetTimer(
                EcologyPounceLockTimer,
                this,
                &USlimeCheatManager::AdvanceEcologyPounceLockTest,
                1.25f,
                false);
}

void USlimeCheatManager::AdvanceEcologyPounceLockTest()
{
        ASlimeEcologyEnemy* Enemy = EcologyCoreTestCore.Get();
        USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
        USlimeBodyInteractionComponent* Body = Enemy ? Enemy->GetBodyInteraction() : nullptr;
        UWorld* World = GetWorld();
        if (!Enemy || !Core || !Body || !World)
        {
                return;
        }

        if (EcologyPounceLockStage == 0)
        {
                APawn* PlayerPawn =
                        GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
                Body->SetAimLocked(true, PlayerPawn);
                EcologyPounceLockStartZ = Enemy->GetActorLocation().Z;
                UE_LOG(LogSlimeWar, Log,
                        TEXT("SlimeEcologyPounceLockTest: locked at phase=%s z=%.1f vz=%.1f."),
                        Core->GetPhaseName(),
                        EcologyPounceLockStartZ,
                        Enemy->GetVelocity().Z);
                EcologyPounceLockStage = 1;
                World->GetTimerManager().SetTimer(
                        EcologyPounceLockTimer,
                        this,
                        &USlimeCheatManager::AdvanceEcologyPounceLockTest,
                        0.8f,
                        false);
                return;
        }

        UE_LOG(LogSlimeWar, Log,
                TEXT("SlimeEcologyPounceLockTest: after 0.8 s zDelta=%.1f vz=%.1f phase=%s aimLocked=%s."),
                Enemy->GetActorLocation().Z - EcologyPounceLockStartZ,
                Enemy->GetVelocity().Z,
                Core->GetPhaseName(),
                Body->IsAimLocked() ? TEXT("yes") : TEXT("no"));
        Body->SetAimLocked(false);

        if (APlayerController* PlayerController = GetPlayerController())
        {
                PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
        }
}

void USlimeCheatManager::AdvanceEcologyCoreTest()
{
	SlimeEcologyCoreStatus();
	++EcologyCoreTestStage;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (EcologyCoreTestStage == 1)
	{
		World->GetTimerManager().SetTimer(
			EcologyCoreTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyCoreTest,
			1.f,
			false);
	}
	else if (EcologyCoreTestStage == 2)
	{
		World->GetTimerManager().SetTimer(
			EcologyCoreTestTimer,
			this,
			&USlimeCheatManager::AdvanceEcologyCoreTest,
			1.5f,
			false);
	}
	else if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeEcologyDevourTest()
{
	SlimeSpawnEcology(2, static_cast<int32>(ESlimeFaction::A), 2);
	RefreshEcologyTestList();
	ASlimeEcologyEnemy* Attacker =
		EcologyTestSlimes.IsValidIndex(0) ? EcologyTestSlimes[0].Get() : nullptr;
	ASlimeEcologyEnemy* Victim =
		EcologyTestSlimes.IsValidIndex(1) ? EcologyTestSlimes[1].Get() : nullptr;
	if (!Attacker || !Victim)
	{
		return;
	}

	USlimeCoreComponent* Core = Attacker->GetCoreComponent();
	if (!Core)
	{
		return;
	}

	Victim->SetFaction(ESlimeFaction::B);
	Core->ArmCore(ESlimeCoreState::Hostile);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyDevourTest: before attacker mass=%d hp=%.1f credit=%d cargo=%d; ")
		TEXT("victim mass=%d credit=%d cargo=%d."),
		Attacker->GetMass(),
		Attacker->GetHealthComponent()->GetHealth(),
		Attacker->GetBodyScoreCredit(),
		Attacker->GetCargo(),
		Victim->GetMass(),
		Victim->GetBodyScoreCredit(),
		Victim->GetCargo());
	Core->DebugDevour(Victim);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyDevourTest: after attacker mass=%d hp=%.1f credit=%d cargo=%d."),
		Attacker->GetMass(),
		Attacker->GetHealthComponent()->GetHealth(),
		Attacker->GetBodyScoreCredit(),
		Attacker->GetCargo());
}

void USlimeCheatManager::SlimeEcologyDetonationTest()
{
	SlimeSpawnEcology(3, static_cast<int32>(ESlimeFaction::A), 1);
	RefreshEcologyTestList();
	ASlimeEcologyEnemy* First =
		EcologyTestSlimes.IsValidIndex(0) ? EcologyTestSlimes[0].Get() : nullptr;
	ASlimeEcologyEnemy* Second =
		EcologyTestSlimes.IsValidIndex(1) ? EcologyTestSlimes[1].Get() : nullptr;
	ASlimeEcologyEnemy* Third =
		EcologyTestSlimes.IsValidIndex(2) ? EcologyTestSlimes[2].Get() : nullptr;
	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this);
	if (!First || !Second || !Third || !PlayerPawn || !Combat)
	{
		return;
	}

	First->ApplyStatRow(2);
	Second->ApplyStatRow(2);
	FVector Near = First->GetActorLocation() + FVector(120.f, 0.f, 0.f);
	Near.Z = Second->GetActorLocation().Z;
	Second->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
	FVector BystanderLocation = First->GetActorLocation() + FVector(0.f, 120.f, 0.f);
	BystanderLocation.Z = Third->GetActorLocation().Z;
	Third->SetActorLocation(BystanderLocation, false, nullptr, ETeleportType::TeleportPhysics);
	First->GetCoreComponent()->ArmCore(ESlimeCoreState::Neutral);
	Second->GetCoreComponent()->ArmCore(ESlimeCoreState::Neutral);
	const float BystanderHealthBefore = Third->GetHealthComponent()->GetHealth();
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyDetonationTest: killing %s next to core %s and bystander %s hp %.1f."),
		*GetNameSafe(First),
		*GetNameSafe(Second),
		*GetNameSafe(Third),
		BystanderHealthBefore);
	Combat->ApplyDamageTo(First, First->GetHealthComponent()->GetMaxHealth(), PlayerPawn);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyDetonationTest: firstDead=%s secondDead=%s ")
		TEXT("bystander hp %.1f -> %.1f external %.1f pendingKill=%s"),
		First->GetHealthComponent()->IsDead() ? TEXT("yes") : TEXT("no"),
		Second->GetHealthComponent()->IsDead() ? TEXT("yes") : TEXT("no"),
		BystanderHealthBefore,
		Third->GetHealthComponent()->GetHealth(),
		Third->GetBodyInteraction()->GetExternalVelocity().Size2D(),
		Third->IsActorBeingDestroyed() ? TEXT("yes") : TEXT("no"));
}

void USlimeCheatManager::SlimeEcologyWeakpointTest()
{
	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 6);
	ASlimeEcologyEnemy* Mass6 = FindNearestEcologyEnemy();
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyWeakpointTest: mass6 weakpoint=%s."),
		Mass6 && Mass6->GetWeakpointComponent()->IsWeakpointActive() ? TEXT("yes") : TEXT("no"));

	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 7);
	ASlimeEcologyEnemy* Mass7 = FindNearestEcologyEnemy();
	USlimeWeakpointComponent* Weakpoint = Mass7 ? Mass7->GetWeakpointComponent() : nullptr;
	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this);
	if (!Mass7 || !Weakpoint || !PlayerPawn || !Combat)
	{
		return;
	}

	const float Before = Mass7->GetHealthComponent()->GetHealth();
	Combat->ApplyDamageToComponent(Mass7, Weakpoint, 0.f, PlayerPawn);
	const float AfterZero = Mass7->GetHealthComponent()->GetHealth();
	Combat->ApplyDamageToComponent(Mass7, Weakpoint, 1.f, PlayerPawn);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyWeakpointTest: mass7 active=%s hp before=%.1f afterZero=%.1f dead=%s."),
		Weakpoint->IsWeakpointActive() ? TEXT("yes") : TEXT("no"),
		Before,
		AfterZero,
		Mass7->GetHealthComponent()->IsDead() ? TEXT("yes") : TEXT("no"));
}

void USlimeCheatManager::SlimeWeaponStatus()
{
	ASlimeWarCharacter* Character =
		GetPlayerController() ? Cast<ASlimeWarCharacter>(GetPlayerController()->GetPawn()) : nullptr;
	const USlimeWeaponRules* Rules = USlimeWeaponRules::GetFor(this);
	const USlimeWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	const USlimeAimAssistComponent* Aim = Character ? Character->GetAimAssistComponent() : nullptr;
	if (!Character || !Rules || !Weapon || !Aim)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeWeaponStatus: player weapon/aim components unavailable."));
		return;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("WeaponStatus: ammo=%d/%d interval=%.3f range=%.0f charge=%.2f charging=%s ")
		TEXT("locked=%s weakpoint=%s screenDistance=%.1f captureRadius=%.1f"),
		Weapon->GetMagazineAmmo(),
		Weapon->GetMagazineSize(),
		Rules->FireInterval,
		Rules->Range,
		Character->GetChargeAlpha(),
		Character->IsCharging() ? TEXT("yes") : TEXT("no"),
		*GetNameSafe(Aim->GetLockedTarget()),
		Aim->IsWeakpointLocked() ? TEXT("yes") : TEXT("no"),
		Aim->GetLastScreenDistance(),
		Aim->GetLastCaptureRadius());
}

void USlimeCheatManager::SlimeAimAssistStatus()
{
	ASlimeWarCharacter* Character =
		GetPlayerController() ? Cast<ASlimeWarCharacter>(GetPlayerController()->GetPawn()) : nullptr;
	const USlimeAimAssistComponent* Aim = Character ? Character->GetAimAssistComponent() : nullptr;
	if (!Aim)
	{
		return;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("AimAssist: locked=%s component=%s weakpoint=%s aimPoint=%s ")
		TEXT("screenDistance=%.1f captureRadius=%.1f"),
		*GetNameSafe(Aim->GetLockedTarget()),
		*GetNameSafe(Aim->GetLockedComponent()),
		Aim->IsWeakpointLocked() ? TEXT("yes") : TEXT("no"),
		*Aim->GetLockedAimPoint().ToCompactString(),
		Aim->GetLastScreenDistance(),
		Aim->GetLastCaptureRadius());
}

void USlimeCheatManager::SlimeAimAssistScreenTest()
{
	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 1);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AimAssistTestTimer,
			this,
			&USlimeCheatManager::FinishAimAssistScreenTest,
			0.5f,
			false);
	}
}

void USlimeCheatManager::FinishAimAssistScreenTest()
{
	SlimeAimAssistStatus();
	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeChargeTest(float Alpha)
{
	ASlimeWarCharacter* Character =
		GetPlayerController() ? Cast<ASlimeWarCharacter>(GetPlayerController()->GetPawn()) : nullptr;
	USlimeAimAssistComponent* Aim = Character ? Character->GetAimAssistComponent() : nullptr;
	if (!Character || !Aim)
	{
		return;
	}

	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), 2);
	ASlimeEcologyEnemy* Target = FindNearestEcologyEnemy();
	USlimeHealthComponent* Health = Target ? Target->GetHealthComponent() : nullptr;
	USlimeBodyInteractionComponent* Body = Target ? Target->GetBodyInteraction() : nullptr;
	if (!Target || !Health || !Body || !Aim->DebugForceLock(Target, false))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeChargeTest: target/lock setup failed."));
		return;
	}

	const float BeforeHealth = Health->GetHealth();
	FVector ImpactPoint = FVector::ZeroVector;
	AActor* HitActor = nullptr;
	bool bWeakpoint = false;
	Character->PerformChargedShot(
		FMath::Clamp(Alpha, 0.f, 1.f),
		ImpactPoint,
		HitActor,
		bWeakpoint);

	UE_LOG(LogSlimeWar, Log,
		TEXT("ChargeTest: alpha=%.2f hit=%s weakpoint=%s hp %.1f -> %.1f ")
		TEXT("external=%.1f pendingStun=%.2f impact=%s"),
		FMath::Clamp(Alpha, 0.f, 1.f),
		*GetNameSafe(HitActor),
		bWeakpoint ? TEXT("yes") : TEXT("no"),
		BeforeHealth,
		Health->GetHealth(),
		Body->GetExternalVelocity().Size2D(),
		Body->GetStunPending(),
		*ImpactPoint.ToCompactString());
}

void USlimeCheatManager::SlimeKnockbackTest()
{
	ASlimeWarCharacter* Character =
		GetPlayerController() ? Cast<ASlimeWarCharacter>(GetPlayerController()->GetPawn()) : nullptr;
	USlimeAimAssistComponent* Aim = Character ? Character->GetAimAssistComponent() : nullptr;
	if (!Character || !Aim)
	{
		return;
	}

	SlimeSpawnEcology(3, static_cast<int32>(ESlimeFaction::A), 1);
	ASlimeEcologyEnemy* Target = FindNearestEcologyEnemy();
	if (!Target || !Aim->DebugForceLock(Target, false))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeKnockbackTest: lock setup failed."));
		return;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	AActor* HitActor = nullptr;
	bool bWeakpoint = false;
	Character->PerformChargedShot(1.f, ImpactPoint, HitActor, bWeakpoint);
	SlimeEcologyBodyStatus();
}

void USlimeCheatManager::SlimeImpactSplitTest(int32 Mass)
{
	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), FMath::Clamp(Mass, 2, 8));
	ASlimeEcologyEnemy* Parent = FindNearestEcologyEnemy();
	USlimeHealthComponent* ParentHealth = Parent ? Parent->GetHealthComponent() : nullptr;
	if (!Parent || !ParentHealth)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeImpactSplitTest: parent setup failed."));
		return;
	}

	ParentHealth->InitializeHealth(ParentHealth->GetMaxHealth(), 0.5f);
	Parent->SetEcologyTotals(3, 20);
	const int32 ParentMass = Parent->GetMass();
	const float ParentHealthValue = ParentHealth->GetHealth();
	const bool bSplit = Parent->TryImpactSplit(
		Parent->GetActorLocation() + FVector(80.f, 0.f, 0.f),
		FVector::ForwardVector);

	int32 FragmentCount = 0;
	float FragmentHealth = 0.f;
	int32 FragmentCargo = 0;
	int32 FragmentCredit = 0;
	for (TActorIterator<ASlimeEcologyEnemy> It(GetWorld()); It; ++It)
	{
		const ASlimeEcologyEnemy* Fragment = *It;
		if (Fragment && Fragment->GetMass() == 1)
		{
			++FragmentCount;
			FragmentHealth += Fragment->GetHealthComponent()
				? Fragment->GetHealthComponent()->GetHealth()
				: 0.f;
			FragmentCargo += Fragment->GetCargo();
			FragmentCredit += Fragment->GetBodyScoreCredit();
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("ImpactSplitTest: split=%s parentMass=%d parentHealth=%.1f fragments=%d ")
		TEXT("fragmentHealth=%.1f cargo=%d credit=%d"),
		bSplit ? TEXT("yes") : TEXT("no"),
		ParentMass,
		ParentHealthValue,
		FragmentCount,
		FragmentHealth,
		FragmentCargo,
		FragmentCredit);
}

void USlimeCheatManager::SlimeImpactSplitCollisionTest(int32 Mass)
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = GetPlayerController();
	APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(PlayerPawn);
	USlimeAimAssistComponent* Aim = Character ? Character->GetAimAssistComponent() : nullptr;
	if (!World || !PlayerPawn || !Character || !Aim)
	{
		return;
	}

	SlimeSpawnEcology(1, static_cast<int32>(ESlimeFaction::A), FMath::Clamp(Mass, 2, 8));
	ASlimeEcologyEnemy* Target = FindNearestEcologyEnemy();
	if (!Target || !Aim->DebugForceLock(Target, false))
	{
		return;
	}

	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal2D();
	const FVector WallLocation =
		PlayerPawn->GetActorLocation() + Forward * 1050.f + FVector(0.f, 0.f, 160.f);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(),
		WallLocation,
		PlayerPawn->GetActorRotation(),
		SpawnParameters);
	if (!Wall)
	{
		return;
	}

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* WallMesh = Wall->GetStaticMeshComponent();
	WallMesh->SetMobility(EComponentMobility::Movable);
	WallMesh->SetStaticMesh(Cube);
	WallMesh->SetCollisionObjectType(ECC_WorldStatic);
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
	WallMesh->SetMobility(EComponentMobility::Static);
	Wall->SetActorScale3D(FVector(3.f, 3.f, 5.f));
	ImpactSplitCollisionWall = Wall;
	ImpactSplitCollisionParentMass = Target->GetMass();

	FVector ImpactPoint = FVector::ZeroVector;
	AActor* HitActor = nullptr;
	bool bWeakpoint = false;
	Character->PerformChargedShot(1.f, ImpactPoint, HitActor, bWeakpoint);
	UE_LOG(LogSlimeWar, Log,
		TEXT("ImpactSplitCollisionTest: mass %d pushed toward static cube at %s."),
		ImpactSplitCollisionParentMass,
		*WallLocation.ToCompactString());

	World->GetTimerManager().SetTimer(
		ImpactSplitCollisionTimer,
		this,
		&USlimeCheatManager::FinishImpactSplitCollisionTest,
		1.2f,
		false);
}

void USlimeCheatManager::FinishImpactSplitCollisionTest()
{
	RefreshEcologyTestList();
	int32 MassOneCount = 0;
	int32 RemainingParentCount = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakEnemy : EcologyTestSlimes)
	{
		if (const ASlimeEcologyEnemy* Enemy = WeakEnemy.Get())
		{
			if (Enemy->GetMass() == 1)
			{
				++MassOneCount;
			}
			else if (Enemy->GetMass() == ImpactSplitCollisionParentMass)
			{
				++RemainingParentCount;
			}
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("ImpactSplitCollisionTest: parentMass=%d fragments=%d remainingParents=%d."),
		ImpactSplitCollisionParentMass,
		MassOneCount,
		RemainingParentCount);
	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
	if (AActor* Wall = ImpactSplitCollisionWall.Get())
	{
		Wall->Destroy();
	}
	ImpactSplitCollisionWall.Reset();
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

void USlimeCheatManager::SlimeEcologyResourceStatus()
{
	const UWorld* World = GetWorld();
	const USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Ecology)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("SlimeEcologyResourceStatus: ecology subsystem missing."));
		return;
	}

	for (const FSlimeSiteStatus& Site : Ecology->GetSiteStatuses())
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologySite: %s faction=%d live=%d mass=%d delivered=%d cores=%d pending=%d"),
			*Site.SiteId.ToString(),
			static_cast<int32>(Site.Faction),
			Site.LiveCount,
			Site.LiveMass,
			Site.DeliveredCount,
			Site.CoreCount,
			Site.PendingSpawns);
	}

	for (const FSlimeMineralStatus& Mineral : Ecology->GetMineralStatuses())
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologyMineral: %s stock=%d initial=%d max=%d regen=%s remaining=%.2f"),
			*Mineral.PatchId.ToString(),
			Mineral.Stock,
			Mineral.InitialStock,
			Mineral.MaxStock,
			Mineral.bRegenerates ? TEXT("yes") : TEXT("no"),
			Mineral.RegrowRemaining);
	}

	Ecology->CheckLedger(/*bLogResult=*/true, TEXT("debug status"));
}

void USlimeCheatManager::SlimeEcologyResourceSoakTest(float Seconds)
{
	const UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Ecology || !Ecology->HasEcologySites())
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyResourceSoakTest: no ecology sites in this world."));
		return;
	}

	if (const URunSubsystem* Run = URunSubsystem::Get(this))
	{
		if (!Run->IsRunning())
		{
			SlimeRunStart();
		}
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyResourceSoakTest: started %.1f second resource observation."),
		FMath::Max(1.f, Seconds));
	if (UWorld* MutableWorld = GetWorld())
	{
		MutableWorld->GetTimerManager().SetTimer(
			EcologyResourceSoakTimer,
			this,
			&USlimeCheatManager::FinishEcologyResourceSoak,
			FMath::Max(1.f, Seconds),
			false);
	}
}

void USlimeCheatManager::FinishEcologyResourceSoak()
{
	UE_LOG(LogSlimeWar, Log, TEXT("SlimeEcologyResourceSoakTest: observation finished."));
	SlimeEcologyResourceStatus();
	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("HighResShot 1"));
	}
}

void USlimeCheatManager::SlimeEcologyDeliveryTest()
{
	if (const URunSubsystem* Run = URunSubsystem::Get(this))
	{
		if (!Run->IsRunning())
		{
			SlimeRunStart();
		}
	}

	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	ASlimeFactionSite* Site = Enemy ? Enemy->GetAssignedSite() : nullptr;
	if (!Enemy || !Site)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("SlimeEcologyDeliveryTest: no ecology slime with an assigned site."));
		return;
	}

	USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (Enemy->GetCargo() <= 0
		&& (!Ecology || !Ecology->DebugGrantCargoFromStock(Enemy)))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("SlimeEcologyDeliveryTest: no cargo stock available."));
		return;
	}
	EcologyDeliveryTarget = Enemy;
	EcologyDeliveryBaseline = Site->GetDeliveredCount();

	FVector DeliveryLocation = Site->GetDeliveryLocation() + FVector(80.f, 0.f, 0.f);
	DeliveryLocation.Z = Enemy->GetActorLocation().Z;
	Enemy->SetActorLocation(
		DeliveryLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (USlimeEcologyBrainComponent* Brain = Enemy->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyDeliveryTest: moved %s with cargo to site %s."),
		*GetNameSafe(Enemy),
		*Site->SiteId.ToString());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EcologyDeliveryTestTimer,
			this,
			&USlimeCheatManager::FinishEcologyDeliveryTest,
			1.f,
			false);
	}
}

void USlimeCheatManager::FinishEcologyDeliveryTest()
{
	ASlimeEcologyEnemy* Enemy = EcologyDeliveryTarget.Get();
	ASlimeFactionSite* Site = Enemy ? Enemy->GetAssignedSite() : nullptr;
	if (Site)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("SlimeEcologyDeliveryTest: site delivered delta=%d cargoRemaining=%d."),
			Site->GetDeliveredCount() - EcologyDeliveryBaseline,
			Enemy ? Enemy->GetCargo() : -1);
	}
	SlimeEcologyResourceStatus();
}

void USlimeCheatManager::SlimeEcologyCargoDropTest()
{
	if (const URunSubsystem* Run = URunSubsystem::Get(this))
	{
		if (!Run->IsRunning())
		{
			SlimeRunStart();
		}
	}

	ASlimeEcologyEnemy* Enemy = FindNearestEcologyEnemy();
	APawn* PlayerPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
	USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this);
	if (!Enemy || !Combat)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("SlimeEcologyCargoDropTest: setup failed."));
		return;
	}

	USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (Enemy->GetCargo() <= 0
		&& (!Ecology || !Ecology->DebugGrantCargoFromStock(Enemy)))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("SlimeEcologyCargoDropTest: no cargo stock available."));
		return;
	}
	Combat->ApplyDamageTo(Enemy, 9999.f, PlayerPawn);
	UE_LOG(LogSlimeWar, Log,
		TEXT("SlimeEcologyCargoDropTest: killed %s carrying one cargo unit."),
		*GetNameSafe(Enemy));
	SlimeEcologyResourceStatus();
}

void USlimeCheatManager::SlimeEcologyResourceLedgerCheck()
{
	const UWorld* World = GetWorld();
	const USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (Ecology)
	{
		Ecology->CheckLedger(/*bLogResult=*/true, TEXT("manual check"));
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
			if (ASlimeEcologyEnemy* EcologyEnemy = Cast<ASlimeEcologyEnemy>(Enemy))
			{
				if (USlimeEcologySubsystem* Ecology =
					World->GetSubsystem<USlimeEcologySubsystem>())
				{
					Ecology->NotifyAgentRemoved(EcologyEnemy, EcologyEnemy->GetMass());
				}
			}
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
        UE_LOG(LogSlimeWar, Log,
                TEXT("player kills %d   npc kills %d   cargo scored %d   hostile kills %d ")
                TEXT("detonations %d   chain kills %d   delivered %d   fusions %d"),
                Result.PlayerKills,
                Result.NpcKills,
                Result.PlayerCargoScored,
                Result.HostileKills,
                Result.Detonations,
                Result.ChainKills,
                Result.DeliveredCargo,
                Result.Fusions);
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
