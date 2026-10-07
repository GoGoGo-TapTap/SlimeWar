// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/GameStateBase.h"
#include "SlimeRunGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateValueChanged, int32 /*Value*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateRunStateChanged, ESlimeRunState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateRunEnded, ERunEndReason /*Reason*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeGameStatePointChanged, int32 /*PointId*/, ESpawnPointState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeGameStateBatchIncoming, int32 /*BatchIndex*/, float /*SecondsUntilSpawn*/);

/**
 * Read-only mirror of the run (PC-09).
 *
 * The authority stays in the subsystems (URunSubsystem / UScoreSubsystem); this class only caches
 * their values and re-broadcasts, so a HUD has exactly one place to bind to and one place to read
 * from, and Phase D's widget work never has to know how the run is implemented.
 *
 * Fast changing debug numbers (live enemy counts, peak, current batch) are deliberately NOT mirrored
 * here - the debug HUD asks the subsystems directly for those.
 */
UCLASS()
class ASlimeRunGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	ESlimeRunState GetRunState() const { return RunState; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	ERunEndReason GetEndReason() const { return EndReason; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetCurrentScore() const { return CurrentScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetBestScore() const { return BestScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetTargetScore() const { return TargetScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetRemainingSeconds() const { return RemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetNormalKillCount() const { return NormalKillCount; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetClearedPointCount() const { return ClearedPointCount; }

	/** "Passed" only means the minimum score was reached (design 2.5). */
	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	bool IsTargetReached() const { return CurrentScore >= TargetScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetPointCount() const { return PointIds.Num(); }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetPointIdAt(int32 Index) const { return PointIds.IsValidIndex(Index) ? PointIds[Index] : INDEX_NONE; }

	/** Returns AwaitingDeploy for an unknown point id. */
	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	ESpawnPointState GetPointState(int32 PointId) const;

	// -- UI binding surface --

	FSlimeGameStateValueChanged OnScoreChanged;
	FSlimeGameStateValueChanged OnBestScoreChanged;
	FSlimeGameStateValueChanged OnTimeChanged;
	FSlimeGameStateValueChanged OnNormalKillCountChanged;
	FSlimeGameStateValueChanged OnClearedPointCountChanged;
	FSlimeGameStatePointChanged OnPointStateChanged;
	FSlimeGameStateRunStateChanged OnRunStateChanged;
	FSlimeGameStateRunEnded OnRunEnded;
	FSlimeGameStateBatchIncoming OnBatchIncoming;

protected:
	/** Read everything once, so a HUD that binds late still sees the current run. */
	void PullSnapshot();

	void BindSubsystems();
	void UnbindSubsystems();

	void HandleScoreChanged(int32 NewScore);
	void HandleBestScoreChanged(int32 NewBestScore);
	void HandleNormalKillCountChanged(int32 NewCount);
	void HandleClearedPointCountChanged(int32 NewCount);
	void HandleTimeChanged(int32 NewRemainingSeconds);
	void HandlePointStateChanged(int32 PointId, ESpawnPointState NewState);
	void HandleRunStateChanged(ESlimeRunState NewState);
	void HandleRunEnded(ERunEndReason Reason);
	void HandleBatchIncoming(int32 BatchIndex, float SecondsUntilSpawn);

	void SetPointState(int32 PointId, ESpawnPointState NewState);

private:
	ESlimeRunState RunState = ESlimeRunState::Idle;
	ERunEndReason EndReason = ERunEndReason::None;
	int32 CurrentScore = 0;
	int32 BestScore = 0;
	int32 TargetScore = 0;
	int32 RemainingSeconds = 0;
	int32 NormalKillCount = 0;
	int32 ClearedPointCount = 0;

	TArray<int32> PointIds;
	TArray<ESpawnPointState> PointStates;

	/** Which run world we bound to, so EndPlay only unbinds what we actually bound. */
	TWeakObjectPtr<class URunSubsystem> BoundRunSubsystem;
	TWeakObjectPtr<class UScoreSubsystem> BoundScoreSubsystem;
};
