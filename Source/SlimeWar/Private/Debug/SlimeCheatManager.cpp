// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeCheatManager.h"

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
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
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Math/NumericLimits.h"
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
