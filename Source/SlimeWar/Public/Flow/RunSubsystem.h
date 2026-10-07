// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "RunSubsystem.generated.h"

class ASlimeWarGameMode;
class ASpawnPoint;
class USlimeRunConfig;
class USlimeSpawnLayout;

/**
 * Settlement data (PD-03).
 *
 * Assembled once when the run ends and read by the result screen through ASlimeRunGameState.
 * "Passed" is design 2.5: the countdown ran out with the minimum score met. Dying is a failure
 * even with a high score, and reaching the target never ends the run early.
 */
USTRUCT(BlueprintType)
struct FSlimeRunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	int32 TargetScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	int32 BestScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	int32 NormalKills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	int32 ClearedPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	ERunEndReason EndReason = ERunEndReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Run")
	bool bPassed = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunStateChanged, ESlimeRunState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunEnded, ERunEndReason /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunTimeChanged, int32 /*SecondsRemaining*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeBatchIncoming, int32 /*BatchIndex*/, float /*SecondsUntilSpawn*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimePointStateChanged, int32 /*PointId*/, ESpawnPointState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeFusionHint, FVector /*Location*/);

/**
 * Per-world authority for the run timeline (PC-02 / PC-06).
 *
 * It owns the countdown, the batch schedule and the terminal state; the spawn points own what
 * happens inside one batch. It reaches the enemy / player side only by subscribing to
 * ASlimeWarGameMode's broadcasts (plan red line: GameplayFramework is a lower layer and must not
 * include Flow headers - the subscription happens here, in the upper layer).
 *
 * Phase C: auto-start on world begin play and a minimal terminal state (stop spawning + stop
 * scoring). The deployment cinematic and the result camera are Phase D.
 */
UCLASS()
class URunSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context is not usable. */
	static URunSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Collect the spawn points and start the timeline. Safe to call twice (second call is a no-op). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	void StartRun();

	/**
	 * PD-08: the single way into a run.
	 *
	 * Records the drop point, moves the player onto it and enters Deploying. Nothing spawns and the
	 * countdown does not start until ConfirmDeployment - which is exactly why the deployment
	 * cinematic needs no "world pause" trick (design 5.2: the first batch appears with player
	 * control).
	 *
	 * @return false when the phase is not Idle or the index does not address a drop point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	bool BeginDeployment(int32 DropPointIndex);

	/** Called by the presentation director when the deployment sequence finished. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	void ConfirmDeployment();

	/**
	 * PD-04: finish the attempt and start the next one by reloading the level.
	 *
	 * A reload is what makes "no state left behind" free (the world subsystems are rebuilt and the
	 * GameInstance subsystems keep only what must survive); the intent rides in
	 * USlimeSessionSubsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	void RequestRetry(bool bReselectDropPoint);

	/** Terminal state: stop the batches, lock the score and broadcast (design 5.6). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	void EndRun(ERunEndReason Reason);

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	bool IsRunning() const { return RunState == ESlimeRunState::Running; }

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	ESlimeRunState GetRunState() const { return RunState; }

	/** Alias with the Phase D name; same value as GetRunState. */
	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	ESlimeRunState GetRunPhase() const { return RunState; }

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetSelectedDropPoint() const { return SelectedDropPoint; }

	/** Number of drop points in DA_SpawnLayout (0 when the asset is missing). */
	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetDropPointCount() const;

	/** Settlement data; only meaningful once the run reached Result / Ended. */
	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	FSlimeRunResult GetRunResult() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	float GetElapsedTime() const { return ElapsedTime; }

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetBatchCount() const { return BatchCount; }

	/** Highest batch that has been issued, or INDEX_NONE before the first one. */
	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetLastIssuedBatch() const { return LastIssuedBatch; }

	/** Successful fusions this run (statistics only, never score - design 4.4). */
	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetEnemyFusedCount() const { return EnemyFusedCount; }

	/**
	 * Debug / CP-4 only: jump the countdown to a given remaining time.
	 *
	 * Used to reach the last-15-seconds hint and the time-up ending without waiting three minutes.
	 * Nothing in the game calls this.
	 */
	void DebugSetRemainingSeconds(float SecondsRemaining);

	/** Registered points in ascending PointId order, with their current state. */
	void GetPointSnapshot(TArray<int32>& OutPointIds, TArray<ESpawnPointState>& OutStates) const;

	// -- UI binding surface (ASlimeRunGameState mirrors all of these) --

	FSlimeRunStateChanged OnRunStateChanged;
	FSlimeRunEnded OnRunEnded;
	FSlimeRunTimeChanged OnTimeChanged;
	FSlimeBatchIncoming OnBatchIncoming;
	FSlimePointStateChanged OnPointStateChanged;

	/**
	 * Fires at most once per run: the first fusion that happened close enough to the player for the
	 * "fused slimes are tougher but worth more" hint to make sense (design 7.5 / PD-12).
	 */
	FSlimeFusionHint OnFusionHint;

protected:
	/** One timeline step: advance the clock, warn, issue, count down, end when due. */
	void HandleTimelineTick();

	void HandlePlayerDied();
	void HandleEnemyKilled(ETargetKind Kind, int32 Mass);
	void HandleEnemyFused(int32 ResultMass, const FVector& Location);
	void HandlePointStateChanged(int32 PointId, ESpawnPointState NewState);

	void BindDirectorEvents();
	void UnbindDirectorEvents();

	void CollectSpawnPoints(const USlimeRunConfig& RunConfig);
	void IssueWarnings();
	void IssueDueBatches();
	void SetRunState(ESlimeRunState NewState);
	void BroadcastRemainingSeconds(bool bForce);

	/** Push the current phase onto the player: lock, unlock or enter the result state. */
	void ApplyRunStateToPlayer();

	/** Freeze / release every live enemy for the result camera (plan decision D4). */
	void FreezeEnemies(bool bFrozen);

	/** Result -> Ended once the orbit camera had its time. */
	void FinishResult();

	/** OnWorldBeginPlay path when bAutoStartRun is off: push the phase and honour a retry request. */
	void HandleWorldStartWithoutAutoRun();

	FVector GetDropPointLocation(int32 Index) const;

	const USlimeRunConfig* GetRunConfig() const;

private:
	/** Spawn points found in the level, ascending PointId. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASpawnPoint>> SpawnPoints;

	TWeakObjectPtr<ASlimeWarGameMode> BoundGameMode;

	ESlimeRunState RunState = ESlimeRunState::Idle;
	ERunEndReason EndReason = ERunEndReason::None;
	int32 SelectedDropPoint = INDEX_NONE;
	float ElapsedTime = 0.f;
	float RunDuration = 0.f;
	int32 BatchCount = 0;
	float BatchInterval = 0.f;
	float WarningLead = 0.f;

	/** Next batch waiting to be issued. */
	int32 NextBatchToIssue = 0;
	/** Last batch that already fired its "incoming" warning. Batch 0 never warns. */
	int32 LastWarnedBatch = 0;
	int32 LastIssuedBatch = INDEX_NONE;
	/** Last whole second pushed to the UI, so the countdown does not broadcast every tick. */
	int32 LastBroadcastSecond = INDEX_NONE;
	int32 EnemyFusedCount = 0;

	/** The proximity hint is a once-per-run thing; the UI does not need to re-check it. */
	bool bFusionHintIssued = false;

	FTimerHandle TimelineTimerHandle;
	FTimerHandle ResultTimerHandle;
};
