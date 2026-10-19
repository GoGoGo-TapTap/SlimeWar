// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"

/**
 * Actor-free helpers behind the run flow (scoring rules and legal phase transitions).
 *
 * Everything here is deliberately plain C++ so the rules that must not silently drift live in one
 * place and are covered by Private/Tests/SlimeFlowMathTests.cpp.
 *
 * No numbers are baked in here either: every threshold arrives from USlimeRunConfig.
 */
namespace SlimeFlowMath
{
	inline int32 EcologyKillScore(const FSlimeKillContext& Context)
	{
		if (!Context.bKilledByPlayer)
		{
			return 0;
		}

		const int32 Mass = FMath::Max(1, Context.Mass);
		const int32 LargeBonus = FMath::Max(0, Mass - 2);
		const int32 Body = FMath::Max(
			Mass * 4 + LargeBonus * LargeBonus,
			FMath::Max(0, Context.BodyScoreCredit));
		const int32 Full = Body + FMath::Max(0, Context.Cargo) * 6;
		return Context.bHostileCore ? Full / 2 : Full;
	}

	/**
	 * Legal run phase moves (Phase D).
	 *
	 * The run only ever moves forward through the lifecycle, so every other pair is a bug worth
	 * catching rather than a state to support. Idle -> Running is allowed on purpose: the
	 * SlimeRunStart cheat skips the deployment cinematic, and CP-3 regression relies on it.
	 */
	inline bool IsValidRunPhaseTransition(ESlimeRunState From, ESlimeRunState To)
	{
		switch (From)
		{
		case ESlimeRunState::Idle:
			return To == ESlimeRunState::Deploying || To == ESlimeRunState::Running;
		case ESlimeRunState::Deploying:
			return To == ESlimeRunState::Running;
		case ESlimeRunState::Running:
			return To == ESlimeRunState::Result;
		case ESlimeRunState::Result:
			return To == ESlimeRunState::Ended;
		case ESlimeRunState::Ended:
			return false;
		}

		return false;
	}

	/**
	 * "Passed" is design 2.5: only running the clock out with the minimum score met counts.
	 * Dying is a failure even with a high score, and reaching the target never ends the run early.
	 */
	inline bool ComputeRunPassed(ERunEndReason Reason, int32 Score, int32 TargetScore)
	{
		return Reason == ERunEndReason::TimeUp && Score >= TargetScore;
	}

	/** A drop point index is usable when it addresses one of the configured drop points. */
	inline bool IsValidDropIndex(int32 Index, int32 DropCount)
	{
		return Index >= 0 && Index < DropCount;
	}

	/**
	 * Best score only refreshes on a cleared run (design 2.5: a failed score never replaces it).
	 * "Cleared" = the countdown ran out while the player was alive, with the minimum score met.
	 * Same rule as ComputeRunPassed, kept as its own name because the score subsystem reads it.
	 */
	inline bool ShouldRefreshBestScore(ERunEndReason Reason, int32 Score, int32 TargetScore)
	{
		return ComputeRunPassed(Reason, Score, TargetScore);
	}

}
