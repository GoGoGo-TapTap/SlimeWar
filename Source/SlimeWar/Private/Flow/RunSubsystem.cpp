// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/RunSubsystem.h"

#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/ScoreSubsystem.h"
#include "Flow/SlimeFlowMath.h"
#include "Flow/SlimeSessionSubsystem.h"
#include "Flow/SpawnPoint.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"
#include "Math/NumericLimits.h"
#include "Player/SlimeWarCharacter.h"
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
	else
	{
		// Phase D: the run waits for the preparation screen (or for a retry request that skips it).
		InWorld.GetTimerManager().SetTimerForNextTick(this, &URunSubsystem::HandleWorldStartWithoutAutoRun);
	}
}

void URunSubsystem::Deinitialize()
{
	UnbindDirectorEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimelineTimerHandle);
		World->GetTimerManager().ClearTimer(ResultTimerHandle);
	}
	TimelineTimerHandle.Invalidate();
	ResultTimerHandle.Invalidate();

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
	// Idle is the SlimeRunStart cheat (CP-3 regression); Deploying is the normal Phase D path.
	// Anything later means the run already started or is over.
	if (RunState != ESlimeRunState::Idle && RunState != ESlimeRunState::Deploying)
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

int32 URunSubsystem::GetDropPointCount() const
{
	const USlimeRunConfig* RunConfig = GetRunConfig();
	const USlimeSpawnLayout* Layout = RunConfig ? RunConfig->SpawnLayout.LoadSynchronous() : nullptr;
	return Layout ? Layout->DropPoints.Num() : 0;
}

FVector URunSubsystem::GetDropPointLocation(int32 Index) const
{
	const USlimeRunConfig* RunConfig = GetRunConfig();
	const USlimeSpawnLayout* Layout = RunConfig ? RunConfig->SpawnLayout.LoadSynchronous() : nullptr;

	if (!Layout || !Layout->DropPoints.IsValidIndex(Index))
	{
		return FVector::ZeroVector;
	}

	return Layout->DropPoints[Index];
}

bool URunSubsystem::BeginDeployment(int32 DropPointIndex)
{
	if (RunState != ESlimeRunState::Idle)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::BeginDeployment: called in state %d, ignored (only Idle may deploy)."),
			static_cast<int32>(RunState));
		return false;
	}

	const int32 DropCount = GetDropPointCount();
	if (!SlimeFlowMath::IsValidDropIndex(DropPointIndex, DropCount))
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::BeginDeployment: drop point %d is out of range (DA_SpawnLayout has %d)."),
			DropPointIndex, DropCount);
		return false;
	}

	SelectedDropPoint = DropPointIndex;

	if (USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this))
	{
		Session->SetSelectedDropPoint(DropPointIndex);
	}

	// Park the pawn on the drop point while the camera dives towards it. The extra Z lets gravity
	// settle it onto the floor; the input lock is what keeps it from wandering off.
	if (ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const FVector DropLocation = GetDropPointLocation(DropPointIndex);
		Character->SetActorLocation(
			DropLocation + FVector(0.f, 0.f, 150.f), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Nothing has spawned and the countdown has not started - StartRun does both, and it is only
	// called from ConfirmDeployment.
	SetRunState(ESlimeRunState::Deploying);

	UE_LOG(LogSlimeWar, Log, TEXT("URunSubsystem: deploying to drop point %d."), DropPointIndex);
	return true;
}

void URunSubsystem::ConfirmDeployment()
{
	if (RunState != ESlimeRunState::Deploying)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::ConfirmDeployment: called in state %d, ignored (only Deploying may confirm)."),
			static_cast<int32>(RunState));
		return;
	}

	// StartRun issues batch 0 and starts the 180 s countdown, so control and the first batch land
	// together (design 5.2).
	StartRun();
}

void URunSubsystem::RequestRetry(bool bReselectDropPoint)
{
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
	if (LevelName.IsEmpty())
	{
		UE_LOG(LogSlimeWar, Error, TEXT("URunSubsystem::RequestRetry: could not resolve the current level name."));
		return;
	}

	if (USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this))
	{
		if (bReselectDropPoint)
		{
			// Back to the preparation screen: no drop point, no auto deploy.
			Session->ClearSelectedDropPoint();
		}
		else
		{
			// Same drop point, straight into the cinematic.
			Session->SetSelectedDropPoint(SelectedDropPoint);
			Session->RequestAutoDeploy();
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("URunSubsystem: reloading '%s' (%s)."), *LevelName,
		bReselectDropPoint ? TEXT("reselect drop point") : TEXT("retry same drop point"));

	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void URunSubsystem::HandleWorldStartWithoutAutoRun()
{
	// The pawn spawns with Controllable by default, so the Idle / Deploying lock has to be pushed
	// once the world is up.
	ApplyRunStateToPlayer();

	USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
	if (!Session || !Session->ConsumeAutoDeployRequest())
	{
		return;
	}

	const int32 DropIndex = Session->GetSelectedDropPoint();
	if (!BeginDeployment(DropIndex))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("URunSubsystem: stored drop point %d could not be reused, showing the preparation screen."),
			DropIndex);
	}
}

FSlimeRunResult URunSubsystem::GetRunResult() const
{
	FSlimeRunResult Result;
	Result.EndReason = EndReason;

	if (const UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Result.Score = Score->GetCurrentScore();
		Result.TargetScore = Score->GetTargetScore();
		Result.BestScore = Score->GetBestScore();
		Result.NormalKills = Score->GetNormalKillCount();
		Result.ClearedPoints = Score->GetClearedPointCount();
	}

	Result.bPassed = SlimeFlowMath::ComputeRunPassed(Result.EndReason, Result.Score, Result.TargetScore);
	return Result;
}

void URunSubsystem::EndRun(ERunEndReason Reason)
{
	// Idempotent: the countdown running out and the player dying can land in the same frame.
	if (RunState == ESlimeRunState::Result || RunState == ESlimeRunState::Ended)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimelineTimerHandle);
	}
	TimelineTimerHandle.Invalidate();

	EndReason = Reason;

	// Design 5.6: after the run ends nothing new is spawned and no new score is earned.
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

	// The result camera needs a still world. Only Flow knows the run just ended, so the freeze is
	// pushed down from here (plan decision D4); SetRunState(Result) also raises
	// State.Player.Result, which cancels a reload that is still in flight (PA-10 run-end half).
	FreezeEnemies(true);
	SetRunState(ESlimeRunState::Result);

	// The numbers are final now, so tell the UI before the camera plays: the settlement screen
	// reads them off ASlimeRunGameState when the phase reaches Ended.
	OnRunEnded.Broadcast(Reason);

	const USlimeRunConfig* RunConfig = GetRunConfig();
	const float OrbitDuration = RunConfig ? FMath::Max(0.f, RunConfig->ResultOrbitDuration) : 0.f;

	if (UWorld* World = GetWorld())
	{
		if (OrbitDuration > 0.f)
		{
			World->GetTimerManager().SetTimer(
				ResultTimerHandle, this, &URunSubsystem::FinishResult, OrbitDuration, /*bLoop=*/false);
		}
		else
		{
			// No orbit configured (asset-less code pass, or a map without a sequence): go straight
			// to the settlement screen instead of leaving the player staring at a frozen world.
			FinishResult();
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("URunSubsystem: run ended (%s) after %.1f s."),
		Reason == ERunEndReason::PlayerDied ? TEXT("player died") : TEXT("time up"),
		ElapsedTime);
}

void URunSubsystem::FinishResult()
{
	if (RunState != ESlimeRunState::Result)
	{
		return;
	}

	SetRunState(ESlimeRunState::Ended);
}

void URunSubsystem::FreezeEnemies(bool bFrozen)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		if (ASlimeEnemyBase* Enemy = *It)
		{
			Enemy->SetRunFrozen(bFrozen);
		}
	}
}

void URunSubsystem::ApplyRunStateToPlayer()
{
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Character)
	{
		return;
	}

	switch (RunState)
	{
	case ESlimeRunState::Running:
		Character->SetRunInputBlocked(false);
		break;

	case ESlimeRunState::Result:
		Character->EnterResultState();
		break;

	case ESlimeRunState::Idle:
	case ESlimeRunState::Deploying:
		Character->SetRunInputBlocked(true);
		break;

	case ESlimeRunState::Ended:
	default:
		// Result already locked the player; Ended adds nothing.
		break;
	}
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

	// The run only ever moves forward. A warning here means two callers raced (which the guards in
	// BeginDeployment / ConfirmDeployment / EndRun should prevent), not that the move is rejected.
	if (!SlimeFlowMath::IsValidRunPhaseTransition(RunState, NewState))
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("URunSubsystem: unexpected run phase move %d -> %d."),
			static_cast<int32>(RunState), static_cast<int32>(NewState));
	}

	RunState = NewState;
	OnRunStateChanged.Broadcast(RunState);

	// Single place that keeps the player in step with the phase.
	ApplyRunStateToPlayer();
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

void URunSubsystem::HandleEnemyFused(int32 /*ResultMass*/, const FVector& Location)
{
	// Fusion is statistics / presentation only; it must never reach the score (design 4.4).
	++EnemyFusedCount;

	if (bFusionHintIssued)
	{
		return;
	}

	const USlimeRunConfig* RunConfig = GetRunConfig();
	const float Proximity = RunConfig ? RunConfig->TutorialFusionProximity : 0.f;

	// Proximity <= 0 means "no range limit": the hint then fires on the first fusion, which is the
	// right fallback for a map where the player is never told about fusions otherwise.
	if (Proximity > 0.f)
	{
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!PlayerPawn || FVector::Dist(PlayerPawn->GetActorLocation(), Location) > Proximity)
		{
			return;
		}
	}

	bFusionHintIssued = true;
	OnFusionHint.Broadcast(Location);
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

void URunSubsystem::DebugSetRemainingSeconds(float SecondsRemaining)
{
	if (RunState != ESlimeRunState::Running)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("URunSubsystem::DebugSetRemainingSeconds: the run is not running, ignored."));
		return;
	}

	ElapsedTime = FMath::Clamp(RunDuration - FMath::Max(0.f, SecondsRemaining), 0.f, RunDuration);
	BroadcastRemainingSeconds(/*bForce=*/true);

	UE_LOG(LogSlimeWar, Log, TEXT("URunSubsystem: debug jump, %d s remaining."), GetRemainingSeconds());
}

int32 URunSubsystem::GetRemainingSeconds() const
{
	return FMath::Max(0, FMath::CeilToInt(GetRemainingTime()));
}
