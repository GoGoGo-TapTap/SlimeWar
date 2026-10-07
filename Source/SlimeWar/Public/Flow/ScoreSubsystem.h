// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScoreSubsystem.generated.h"

/** Generic integer change notification used by the score subsystem. */
DECLARE_MULTICAST_DELEGATE_OneParam(FSlimeScoreValueChanged, int32 /*Value*/);

/**
 * Run score and the session best score (PC-07).
 *
 * GameInstance scoped on purpose: the run counters reset on every run, but the best score has to
 * survive a retry (design 2.5 / plan decision "进程内跨重试保留"), and a retry reloads the level.
 *
 * Everything the player earns flows through IBattleDirector -> URunSubsystem -> here, so the
 * enemy and the weapon never learn that a score system exists.
 */
UCLASS()
class UScoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context is not usable. */
	static UScoreSubsystem* Get(const UObject* WorldContextObject);

	/** Reset the run counters. The best score deliberately survives. */
	void BeginRun(int32 InTargetScore);

	/** Add the score of one kill. Ignored for aggressive targets and after the run ended. */
	void AddKill(ETargetKind Kind, int32 Mass);

	/** One more spawn point reached "cleared" (settlement data, design 2.6). */
	void AddClearedPoint();

	/** No further score can be earned this run (design 5.6: 结束关卡后停止新增得分). */
	void LockScoring() { bScoreLocked = true; }

	/**
	 * Debug / CP-4 only: add score directly, bypassing the mass table.
	 *
	 * Used to check that crossing the target score does NOT end the run early. Kill counts are
	 * deliberately untouched, so the per-mass breakdown stays honest.
	 */
	void DebugAddScore(int32 Points);

	/** Fold the finished run into the best score. Only a cleared run refreshes it. */
	void ResolveBestScore(ERunEndReason Reason);

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	int32 GetCurrentScore() const { return CurrentScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	int32 GetBestScore() const { return BestScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	int32 GetTargetScore() const { return TargetScore; }

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	int32 GetNormalKillCount() const { return NormalKillCount; }

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	int32 GetClearedPointCount() const { return ClearedPointCount; }

	UFUNCTION(BlueprintPure, Category = "Slime|Score")
	bool IsScoringLocked() const { return bScoreLocked; }

	/**
	 * Kills and score per mass tier, index = mass (0 unused).
	 *
	 * This exists so CP-3 can compare the live total against a hand calculation per mass tier
	 * instead of only trusting one number.
	 */
	void GetKillBreakdown(TArray<int32>& OutKillsByMass, TArray<int32>& OutScoreByMass) const;

	FSlimeScoreValueChanged OnScoreChanged;
	FSlimeScoreValueChanged OnNormalKillCountChanged;
	FSlimeScoreValueChanged OnClearedPointCountChanged;
	FSlimeScoreValueChanged OnBestScoreChanged;

protected:
	virtual void Deinitialize() override;

private:
	/** Mass tiers tracked by GetKillBreakdown (design: mass 1..8). */
	static constexpr int32 MaxTrackedMass = 8;

	int32 CurrentScore = 0;
	int32 BestScore = 0;
	int32 TargetScore = 0;
	int32 NormalKillCount = 0;
	int32 ClearedPointCount = 0;
	bool bScoreLocked = false;

	TArray<int32> KillsByMass;
	TArray<int32> ScoreByMass;
};
