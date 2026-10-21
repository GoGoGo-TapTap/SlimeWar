// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Flow/RunSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "SlimeRunGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateValueChanged, int32 /*Value*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateRunStateChanged, ESlimeRunState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateRunEnded, ERunEndReason /*Reason*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeGameStateFusionHint, FVector /*Location*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimeGameStateRunStateChangedBP, ESlimeRunState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimeGameStatePauseStateChangedBP, bool, bPaused);

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
UCLASS(BlueprintType)
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

	/** Settlement data (PD-03). Only meaningful once the phase reached Result / Ended. */
	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	FSlimeRunResult GetRunResult() const;

	/** Which drop point this attempt used, or INDEX_NONE while the player is still choosing. */
	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetSelectedDropPoint() const { return SelectedDropPoint; }

	// -- UI binding surface --

	FSlimeGameStateValueChanged OnScoreChanged;
	FSlimeGameStateValueChanged OnBestScoreChanged;
	FSlimeGameStateValueChanged OnTimeChanged;
	FSlimeGameStateValueChanged OnNormalKillCountChanged;
	FSlimeGameStateValueChanged OnClearedPointCountChanged;
	FSlimeGameStateRunStateChanged OnRunStateChanged;
	FSlimeGameStateRunEnded OnRunEnded;

	/** Blueprint hook for BGM / camera / ambient changes that follow the run phase. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Run")
	FSlimeGameStateRunStateChangedBP BP_OnRunStateChanged;

	/** Blueprint hook for pausing/resuming continuous media (BGM) without polling. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Run")
	FSlimeGameStatePauseStateChangedBP BP_OnPauseStateChanged;

	/** Fires at most once per run: the first fusion close enough for the tutorial hint (PD-12). */
	FSlimeGameStateFusionHint OnFusionHint;

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
	void HandleRunStateChanged(ESlimeRunState NewState);
	void HandleRunEnded(ERunEndReason Reason);
	void HandleFusionHint(FVector Location);

private:
	ESlimeRunState RunState = ESlimeRunState::Idle;
	ERunEndReason EndReason = ERunEndReason::None;
	int32 CurrentScore = 0;
	int32 BestScore = 0;
	int32 TargetScore = 0;
	int32 RemainingSeconds = 0;
	int32 NormalKillCount = 0;
	int32 ClearedPointCount = 0;
	int32 SelectedDropPoint = INDEX_NONE;

	/** Which run world we bound to, so EndPlay only unbinds what we actually bound. */
	TWeakObjectPtr<class URunSubsystem> BoundRunSubsystem;
	TWeakObjectPtr<class UScoreSubsystem> BoundScoreSubsystem;
};
