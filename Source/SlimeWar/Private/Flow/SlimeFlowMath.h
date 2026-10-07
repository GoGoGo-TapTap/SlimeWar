// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"

/**
 * Actor-free helpers behind Phase C (batch timeline, scoring rules, point state, retry window).
 *
 * Everything here is deliberately plain C++: the spawn points and the run subsystem own the
 * world side, while the rules that must not silently drift live in one place and are covered by
 * Private/Tests/SlimeFlowMathTests.cpp (CP-3 "score matches the hand calculation").
 *
 * No numbers are baked in here either: every threshold arrives from USlimeRunConfig.
 */
namespace SlimeFlowMath
{
	/** Batch timeline: batch i is spawned at i * BatchInterval and warned about earlier. */
	struct FRunSchedule
	{
		int32 BatchCount = 0;
		float BatchInterval = 0.f;
		float WarningLead = 0.f;

		bool IsValid() const { return BatchCount > 0 && BatchInterval > 0.f; }

		/** Seconds into the run at which this batch spawns. */
		float GetBatchTime(int32 BatchIndex) const { return BatchInterval * static_cast<float>(BatchIndex); }

		/**
		 * Seconds into the run at which this batch's warning fires.
		 * Negative for batch 0 (it is already due at t = 0, so it never warns).
		 */
		float GetWarningTime(int32 BatchIndex) const { return GetBatchTime(BatchIndex) - WarningLead; }

		/** Time of the last batch: "supply has been issued" is measured against this. */
		float GetSupplyEndTime() const { return GetBatchTime(BatchCount - 1); }

		/** Highest batch index whose spawn time has been reached, or INDEX_NONE. */
		int32 GetLatestDueBatch(float ElapsedTime) const
		{
			if (!IsValid() || ElapsedTime < 0.f)
			{
				return INDEX_NONE;
			}

			const int32 Index = FMath::FloorToInt(ElapsedTime / BatchInterval);
			return FMath::Clamp(Index, INDEX_NONE, BatchCount - 1);
		}

		/**
		 * The batch that should warn right now and has not warned yet, or INDEX_NONE.
		 * Batch 0 never warns (its lead time is in the past when the run starts).
		 */
		int32 GetPendingWarningBatch(float ElapsedTime, int32 LastWarnedBatch) const
		{
			if (!IsValid() || BatchCount <= 1)
			{
				return INDEX_NONE;
			}

			const int32 Next = FMath::Max(LastWarnedBatch + 1, 1);
			if (Next >= BatchCount)
			{
				return INDEX_NONE;
			}

			return ElapsedTime >= GetWarningTime(Next) ? Next : INDEX_NONE;
		}
	};

	/** Total supply of one point over the whole run (the hard cap of design 5.5). */
	inline int32 TotalSupply(int32 BatchCount, int32 PerBatch)
	{
		return FMath::Max(0, BatchCount) * FMath::Max(0, PerBatch);
	}

	/** Score for one kill. Aggressive targets never score (design 4.6.3). */
	inline int32 ScoreForKill(ETargetKind Kind, int32 KillScoreFromTable)
	{
		return Kind == ETargetKind::Normal ? FMath::Max(0, KillScoreFromTable) : 0;
	}

	/**
	 * Best score only refreshes on a cleared run (design 2.5: a failed score never replaces it).
	 * "Cleared" = the countdown ran out while the player was alive, with the minimum score met.
	 */
	inline bool ShouldRefreshBestScore(ERunEndReason Reason, int32 Score, int32 TargetScore)
	{
		return Reason == ERunEndReason::TimeUp && Score >= TargetScore;
	}

	/**
	 * Point state from the counters the spawn point owns.
	 * @param bSupplyDone    every batch was issued (and no delayed spawn is still pending)
	 * @param bAnyBatchIssued  at least one batch has been issued
	 * @param LiveNormal     live normal slimes belonging to this point (aggro never counts)
	 */
	inline ESpawnPointState EvaluatePointState(bool bSupplyDone, bool bAnyBatchIssued, int32 LiveNormal)
	{
		if (bSupplyDone)
		{
			return LiveNormal > 0 ? ESpawnPointState::DepletedNotCleared : ESpawnPointState::Cleared;
		}

		return bAnyBatchIssued ? ESpawnPointState::Spawning : ESpawnPointState::AwaitingDeploy;
	}

	/** Retries that fit into the delay window. Always at least one attempt. */
	inline int32 GetRetryAttempts(float Window, float Interval)
	{
		if (Interval <= 0.f)
		{
			return 1;
		}

		return FMath::Max(1, FMath::CeilToInt(FMath::Max(0.f, Window) / Interval) + 1);
	}
}
