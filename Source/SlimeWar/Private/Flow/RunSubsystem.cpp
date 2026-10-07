// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/RunSubsystem.h"

#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/ScoreSubsystem.h"
#include "Flow/SlimeFlowMath.h"
#include "Flow/SpawnPoint.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Math/NumericLimits.h"
#include "TimerManager.h"

namespace
{
	/**
	 * Timeline granularity. Small enough that a batch never lands visibly late, and it is the only
	 * clock in Phase C (the countdown, the warnings and the batches all read one ElapsedTime).
	 */
	constexpr float TimelineTickInterval = 0.1f;
}

URunSubsystem* URunSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<URunSubsystem>() : nullptr;
}

void URunSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	BindDirectorEvents();

	// UWorldSubsystem::OnWorldBeginPlay runs BEFORE GameMode::StartPlay(), so the GameState and the
	// spawn point actors have not had their BeginPlay yet. Deferring by one frame lets them pull
	// their first snapshot and register themselves.
	const USlimeRunConfig* RunConfig = GetRunConfig();
	if (RunConfig && RunConfig->bAutoStartRun)
	{
		InWorld.GetTimerManager().SetTimerForNextTick(this, &URunSubsystem::StartRun);
	}
}

void URunSubsystem::Deinitialize()
{
	UnbindDirectorEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimelineTimerHandle);
	}
	TimelineTimerHandle.Invalidate();

	SpawnPoints.Reset();

	Super::Deinitialize();
}

const USlimeRunConfig* URunSubsystem::GetRunConfig() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UStatTableProvider* Provider =
		GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	return Provider ? Provider->GetRunConfig() : nullptr;
}

void URunSubsystem::BindDirectorEvents()
{
	UnbindDirectorEvents();

	UWorld* World = GetWorld();
	ASlimeWarGameMode* GameMode = World ? World->GetAuthGameMode<ASlimeWarGameMode>() : nullptr;
	if (!GameMode)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("URunSubsystem: no ASlimeWarGameMode, the run cannot track combat events."));
		return;
	}

	GameMode->OnEnemyKilledEvent.AddUObject(this, &URunSubsystem::HandleEnemyKilled);
	GameMode->OnEnemyFusedEvent.AddUObject(this, &URunSubsystem::HandleEnemyFused);
	GameMode->OnPointStateChangedEvent.AddUObject(this, &URunSubsystem::HandlePointStateChanged);
	GameMode->OnPlayerDiedEvent.AddUObject(this, &URunSubsystem::HandlePlayerDied);

	BoundGameMode = GameMode;
}

void URunSubsystem::UnbindDirectorEvents()
{
	ASlimeWarGameMode* GameMode = BoundGameMode.Get();
	if (!GameMode)
	{
		return;
	}

	GameMode->OnEnemyKilledEvent.RemoveAll(this);
	GameMode->OnEnemyFusedEvent.RemoveAll(this);
	GameMode->OnPointStateChangedEvent.RemoveAll(this);
	GameMode->OnPlayerDiedEvent.RemoveAll(this);

	BoundGameMode.Reset();
}

void URunSubsystem::CollectSpawnPoints(const USlimeRunConfig& RunConfig)
{
	SpawnPoints.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The layout asset holds the slots, the centre and the radius; the anchor in the map only
	// carries the PointId (plan 7.3: keep the binary map cheap to edit and merge).
	const USlimeSpawnLayout* Layout = RunConfig.SpawnLayout.LoadSynchronous();
	if (!Layout)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem: DA_RunConfig::SpawnLayout is not set, no spawn point can be initialised."));
		return;
	}

	for (TActorIterator<ASpawnPoint> It(World); It; ++It)
	{
		ASpawnPoint* Point = *It;
		if (!Point)
		{
			continue;
		}

		const FSlimeSpawnPointDef* Definition = Layout->Points.FindByPredicate(
			[Point](const FSlimeSpawnPointDef& Candidate)
			{
				return Candidate.PointId == Point->PointId;
			});

		if (!Definition)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("URunSubsystem: spawn point %d has no FSlimeSpawnPointDef in DA_SpawnLayout, it stays idle."),
				Point->PointId);
			continue;
		}

		Point->InitializeRuntime(*Definition, RunConfig);
		SpawnPoints.Add(Point);
	}

	// Deterministic order, so the GameState mirror and the logs are stable.
	SpawnPoints.Sort([](const TObjectPtr<ASpawnPoint>& A, const TObjectPtr<ASpawnPoint>& B)
	{
		const int32 IdA = A ? A->PointId : TNumericLimits<int32>::Max();
		const int32 IdB = B ? B->PointId : TNumericLimits<int32>::Max();
		return IdA < IdB;
	});

	if (SpawnPoints.Num() == 0)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem: no ASpawnPoint anchor was found in the level."));
	}
}

void URunSubsystem::StartRun()
{
	if (RunState == ESlimeRunState::Running)
	{
		return;
	}

	const USlimeRunConfig* RunConfig = GetRunConfig();
	if (!RunConfig)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("URunSubsystem::StartRun: no USlimeRunConfig available."));
		return;
	}

	RunDuration = RunConfig->RunDuration;
	BatchCount = RunConfig->SpawnBatchCount;
	BatchInterval = RunConfig->SpawnBatchInterval;
	WarningLead = RunConfig->SpawnBatchWarningLead;

	if (RunDuration <= 0.f || BatchCount <= 0 || BatchInterval <= 0.f)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::StartRun: RunDuration / SpawnBatchCount / SpawnBatchInterval are not ")
			TEXT("filled in DA_RunConfig, the run does not start."));
		return;
	}

	CollectSpawnPoints(*RunConfig);

	ElapsedTime = 0.f;
	NextBatchToIssue = 0;
	LastWarnedBatch = 0;
	LastIssuedBatch = INDEX_NONE;
	LastBroadcastSecond = INDEX_NONE;
	EnemyFusedCount = 0;

	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->BeginRun(RunConfig->TargetScore);
	}

	SetRunState(ESlimeRunState::Running);

	// Batch 0 is due at t = 0 (design 5.2: the first batch appears with player control).
	IssueDueBatches();
	BroadcastRemainingSeconds(/*bForce=*/true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimelineTimerHandle, this, &URunSubsystem::HandleTimelineTick, TimelineTickInterval, /*bLoop=*/true);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("URunSubsystem: run started - %d points, %d batches every %.0f s, %.0f s total."),
		SpawnPoints.Num(), BatchCount, BatchInterval, RunDuration);
}

void URunSubsystem::EndRun(ERunEndReason Reason)
{
	if (RunState == ESlimeRunState::Ended)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimelineTimerHandle);
	}
	TimelineTimerHandle.Invalidate();

	SetRunState(ESlimeRunState::Ended);

	// Design 5.6: after the run ends nothing new is spawned and no new score is earned. Freezing
	// the enemies and the player is Phase D (it belongs to the result camera, not to the run logic).
	for (const TObjectPtr<ASpawnPoint>& Point : SpawnPoints)
	{
		if (Point)
		{
			Point->StopSpawning();
		}
	}

	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->LockScoring();
		Score->ResolveBestScore(Reason);
	}

	OnRunEnded.Broadcast(Reason);

	UE_LOG(LogSlimeWar, Log, TEXT("URunSubsystem: run ended (%s) after %.1f s."),
		Reason == ERunEndReason::PlayerDied ? TEXT("player died") : TEXT("time up"),
		ElapsedTime);
}

void URunSubsystem::HandleTimelineTick()
{
	if (RunState != ESlimeRunState::Running)
	{
		return;
	}

	// Debug/CP-3 accelerator only (Slime.Run.TimeScale). The data values never change with it.
	const float Scale = FMath::Max(0.f, SlimeCVars::RunTimeScale);
	ElapsedTime += TimelineTickInterval * Scale;

	IssueWarnings();
	IssueDueBatches();
	BroadcastRemainingSeconds(/*bForce=*/false);

	if (ElapsedTime >= RunDuration)
	{
		EndRun(ERunEndReason::TimeUp);
	}
}

void URunSubsystem::IssueWarnings()
{
	SlimeFlowMath::FRunSchedule Schedule;
	Schedule.BatchCount = BatchCount;
	Schedule.BatchInterval = BatchInterval;
	Schedule.WarningLead = WarningLead;

	int32 Batch = Schedule.GetPendingWarningBatch(ElapsedTime, LastWarnedBatch);
	while (Batch != INDEX_NONE)
	{
		// The warning of design 5.2: one second before the next batch, driven by data.
		OnBatchIncoming.Broadcast(Batch, FMath::Max(0.f, Schedule.GetBatchTime(Batch) - ElapsedTime));
		LastWarnedBatch = Batch;
		Batch = Schedule.GetPendingWarningBatch(ElapsedTime, LastWarnedBatch);
	}
}

void URunSubsystem::IssueDueBatches()
{
	SlimeFlowMath::FRunSchedule Schedule;
	Schedule.BatchCount = BatchCount;
	Schedule.BatchInterval = BatchInterval;
	Schedule.WarningLead = WarningLead;

	const int32 DueBatch = Schedule.GetLatestDueBatch(ElapsedTime);

	while (NextBatchToIssue < BatchCount && DueBatch != INDEX_NONE && NextBatchToIssue <= DueBatch)
	{
		for (const TObjectPtr<ASpawnPoint>& Point : SpawnPoints)
		{
			if (Point)
			{
				Point->ExecuteBatch(NextBatchToIssue);
			}
		}

		LastIssuedBatch = NextBatchToIssue;
		++NextBatchToIssue;
	}
}

void URunSubsystem::BroadcastRemainingSeconds(bool bForce)
{
	const int32 Seconds = GetRemainingSeconds();
	if (!bForce && Seconds == LastBroadcastSecond)
	{
		return;
	}

	LastBroadcastSecond = Seconds;
	OnTimeChanged.Broadcast(Seconds);
}

void URunSubsystem::SetRunState(ESlimeRunState NewState)
{
	if (RunState == NewState)
	{
		return;
	}

	RunState = NewState;
	OnRunStateChanged.Broadcast(RunState);
}

void URunSubsystem::HandlePlayerDied()
{
	EndRun(ERunEndReason::PlayerDied);
}

void URunSubsystem::HandleEnemyKilled(ETargetKind Kind, int32 Mass)
{
	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->AddKill(Kind, Mass);
	}
}

void URunSubsystem::HandleEnemyFused(int32 /*ResultMass*/)
{
	// Fusion is statistics / presentation only; it must never reach the score (design 4.4).
	++EnemyFusedCount;
}

void URunSubsystem::HandlePointStateChanged(int32 PointId, ESpawnPointState NewState)
{
	if (NewState == ESpawnPointState::Cleared)
	{
		if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
		{
			Score->AddClearedPoint();
		}
	}

	OnPointStateChanged.Broadcast(PointId, NewState);
}

void URunSubsystem::GetPointSnapshot(TArray<int32>& OutPointIds, TArray<ESpawnPointState>& OutStates) const
{
	OutPointIds.Reset();
	OutStates.Reset();
	OutPointIds.Reserve(SpawnPoints.Num());
	OutStates.Reserve(SpawnPoints.Num());

	for (const TObjectPtr<ASpawnPoint>& Point : SpawnPoints)
	{
		if (!Point)
		{
			continue;
		}

		OutPointIds.Add(Point->PointId);
		OutStates.Add(Point->GetState());
	}
}

float URunSubsystem::GetRemainingTime() const
{
	return FMath::Max(0.f, RunDuration - ElapsedTime);
}

int32 URunSubsystem::GetRemainingSeconds() const
{
	return FMath::Max(0, FMath::CeilToInt(GetRemainingTime()));
}
