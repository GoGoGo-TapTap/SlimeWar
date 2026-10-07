// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "RunSubsystem.generated.h"

class ASlimeWarGameMode;
class ASpawnPoint;
class USlimeRunConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunStateChanged, ESlimeRunState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunEnded, ERunEndReason /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeRunTimeChanged, int32 /*SecondsRemaining*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeBatchIncoming, int32 /*BatchIndex*/, float /*SecondsUntilSpawn*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimePointStateChanged, int32 /*PointId*/, ESpawnPointState /*NewState*/);

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

	/** Terminal state: stop the batches, lock the score and broadcast (design 5.6). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Flow")
	void EndRun(ERunEndReason Reason);

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	bool IsRunning() const { return RunState == ESlimeRunState::Running; }

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	ESlimeRunState GetRunState() const { return RunState; }

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

	/** Registered points in ascending PointId order, with their current state. */
	void GetPointSnapshot(TArray<int32>& OutPointIds, TArray<ESpawnPointState>& OutStates) const;

	// -- UI binding surface (ASlimeRunGameState mirrors all of these) --

	FSlimeRunStateChanged OnRunStateChanged;
	FSlimeRunEnded OnRunEnded;
	FSlimeRunTimeChanged OnTimeChanged;
	FSlimeBatchIncoming OnBatchIncoming;
	FSlimePointStateChanged OnPointStateChanged;

protected:
	/** One timeline step: advance the clock, warn, issue, count down, end when due. */
	void HandleTimelineTick();

	void HandlePlayerDied();
	void HandleEnemyKilled(ETargetKind Kind, int32 Mass);
	void HandleEnemyFused(int32 ResultMass);
	void HandlePointStateChanged(int32 PointId, ESpawnPointState NewState);

	void BindDirectorEvents();
	void UnbindDirectorEvents();

	void CollectSpawnPoints(const USlimeRunConfig& RunConfig);
	void IssueWarnings();
	void IssueDueBatches();
	void SetRunState(ESlimeRunState NewState);
	void BroadcastRemainingSeconds(bool bForce);

	const USlimeRunConfig* GetRunConfig() const;

private:
	/** Spawn points found in the level, ascending PointId. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASpawnPoint>> SpawnPoints;

	TWeakObjectPtr<ASlimeWarGameMode> BoundGameMode;

	ESlimeRunState RunState = ESlimeRunState::Idle;
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

	FTimerHandle TimelineTimerHandle;
};
