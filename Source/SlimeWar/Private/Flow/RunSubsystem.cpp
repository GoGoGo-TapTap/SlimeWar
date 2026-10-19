// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/RunSubsystem.h"

#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/ScoreSubsystem.h"
#include "Ecology/SlimePaintSubsystem.h"
#include "Flow/SlimeFlowMath.h"
#include "Flow/SlimeFlowNames.h"
#include "Flow/SlimeSessionSubsystem.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeLevelDefinition.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"
#include "Math/NumericLimits.h"
#include "Player/SlimeWarCharacter.h"
#include "TimerManager.h"

namespace
{
	/**
	 * Timeline granularity for the countdown and the extinction check.
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

	if (UWorld* World = GetWorld())
	{
		if (USlimeEcologySubsystem* Ecology = World->GetSubsystem<USlimeEcologySubsystem>())
		{
			BoundEcology = Ecology;
			Ecology->OnExtinction.AddUObject(this, &URunSubsystem::HandleEcologyExtinct);
		}
	}

	// The selected level decides which RunConfig this world runs with, so the switch has to happen
	// before anything reads bAutoStartRun, a duration or a stat. Everything downstream (the run
	// timeline, the player, the enemies, StateTree) reads the provider, never the settings, so this
	// one call is what makes the whole config chain per-level.
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UStatTableProvider* Provider = GameInstance->GetSubsystem<UStatTableProvider>())
		{
			const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
			FName LevelId = Session ? Session->GetSelectedLevelId() : NAME_None;
			if (LevelId.IsNone())
			{
				const FString CurrentMap =
					UGameplayStatics::GetCurrentLevelName(&InWorld, /*bRemovePrefixString=*/true);
				if (const USlimeLevelDefinition* DirectLevel =
					GetDefault<USlimeGameSettings>()->FindLevelByMapName(CurrentMap))
				{
					LevelId = DirectLevel->LevelId;
				}
			}
			Provider->SelectLevel(LevelId);
		}
	}

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
	if (USlimeEcologySubsystem* Ecology = BoundEcology.Get())
	{
		Ecology->OnExtinction.RemoveAll(this);
	}
	BoundEcology.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimelineTimerHandle);
		World->GetTimerManager().ClearTimer(ResultTimerHandle);
	}
	TimelineTimerHandle.Invalidate();
	ResultTimerHandle.Invalidate();

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
	GameMode->OnPlayerDiedEvent.RemoveAll(this);

	BoundGameMode.Reset();
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

	if (RunDuration <= 0.f)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::StartRun: RunDuration is not configured."));
		return;
	}

	if (USlimeEcologySubsystem* Ecology = BoundEcology.Get())
	{
		if (!Ecology->HasEcologySites())
		{
			UE_LOG(LogSlimeWar, Error,
				TEXT("URunSubsystem::StartRun: ecology run has no ASlimeFactionSite actors."));
			return;
		}
		Ecology->BeginRun();
	}
	else
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("URunSubsystem::StartRun: ecology subsystem is unavailable."));
		return;
	}

	ElapsedTime = 0.f;
	LastBroadcastSecond = INDEX_NONE;
	EnemyFusedCount = 0;

	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->BeginRun(RunConfig->TargetScore);
	}
        if (UWorld* World = GetWorld())
        {
                if (USlimePaintSubsystem* Paint = World->GetSubsystem<USlimePaintSubsystem>())
                {
                        Paint->BeginRun();
                }
        }

	SetRunState(ESlimeRunState::Running);

	BroadcastRemainingSeconds(/*bForce=*/true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimelineTimerHandle, this, &URunSubsystem::HandleTimelineTick, TimelineTickInterval, /*bLoop=*/true);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("URunSubsystem: run started - ecology mode, %.0f s total."),
		RunDuration);
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
			// Back to the preparation screen: no drop point, but the next world must still enter the
			// game - without this flag USlimeUISubsystem would treat the reload as a fresh launch and
			// show the main menu instead of the preparation screen.
			Session->ClearSelectedDropPoint();
			Session->RequestEnterGameplay();
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
	Result.ElapsedTime = ElapsedTime;

	if (const ASlimeWarCharacter* Character =
		Cast<ASlimeWarCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Result.PlayerDamageTaken = Character->GetTotalDamageTaken();
	}

	if (const UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Result.Score = Score->GetCurrentScore();
		Result.TargetScore = Score->GetTargetScore();
		Result.BestScore = Score->GetBestScore();
		Result.NormalKills = Score->GetNormalKillCount();
                Result.PlayerKills = Score->GetPlayerKillCount();
                Result.NpcKills = Score->GetNpcKillCount();
                Result.PlayerCargoScored = Score->GetPlayerCargoScored();
                Result.HostileKills = Score->GetHostileKillCount();
                Result.Detonations = Score->GetDetonationKillCount();
                Result.ChainKills = Score->GetChainKillCount();
		Result.ClearedPoints = Score->GetClearedPointCount();
                Result.Fusions = EnemyFusedCount;
                if (const USlimeEcologySubsystem* Ecology = BoundEcology.Get())
                {
                        Result.DeliveredCargo = Ecology->GetDeliveredCargo();
                }
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

	if (USlimeEcologySubsystem* Ecology = BoundEcology.Get())
	{
		Ecology->EndRun();
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
		SlimeFlowNames::EndReason(Reason),
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

	BroadcastRemainingSeconds(/*bForce=*/false);

	if (ElapsedTime >= RunDuration)
	{
		EndRun(ERunEndReason::TimeUp);
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

void URunSubsystem::HandleEcologyExtinct()
{
	EndRun(ERunEndReason::Extinction);
}

void URunSubsystem::HandleEnemyKilled(const FSlimeKillContext& Context)
{
	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
                Score->AddKill(Context);
	}
}

void URunSubsystem::HandleEnemyFused(const FSlimeFusionEvent& Event)
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
                if (!PlayerPawn || FVector::Dist(PlayerPawn->GetActorLocation(), Event.Location) > Proximity)
		{
			return;
		}
	}

	bFusionHintIssued = true;
        OnFusionHint.Broadcast(Event.Location);
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
