// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Flow/SlimeFlowMath.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Pure-logic coverage for Phase C (CP-3).
 *
 * These tests intentionally touch no UObject and no world: the batch timeline, the scoring
 * rules, the point state machine and the retry window are the parts that must not silently
 * drift, and they are the parts a hand calculation in the CP-3 checklist compares against.
 */

namespace
{
	/** The design values (plan 6.4 / design 5.2): 6 batches every 20 s, warning 1 s ahead. */
	SlimeFlowMath::FRunSchedule MakeDesignSchedule()
	{
		SlimeFlowMath::FRunSchedule Schedule;
		Schedule.BatchCount = 6;
		Schedule.BatchInterval = 20.f;
		Schedule.WarningLead = 1.f;
		return Schedule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeRunScheduleTest,
	"SlimeWar.Flow.RunSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeRunScheduleTest::RunTest(const FString& Parameters)
{
	const SlimeFlowMath::FRunSchedule Schedule = MakeDesignSchedule();

	TestTrue(TEXT("design schedule is valid"), Schedule.IsValid());
	TestEqual(TEXT("last batch is at 100 s"), Schedule.GetSupplyEndTime(), 100.f);

	// Batches land on 0 / 20 / 40 / 60 / 80 / 100 s.
	const float ExpectedTimes[] = { 0.f, 20.f, 40.f, 60.f, 80.f, 100.f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedTimes); ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("batch %d time"), Index),
			Schedule.GetBatchTime(Index),
			ExpectedTimes[Index]);
	}

	// The warning of batch 0 is before the run starts, so it never fires.
	TestEqual(TEXT("batch 0 warning is in the past"), Schedule.GetWarningTime(0), -1.f);
	TestEqual(TEXT("batch 1 warns at 19 s"), Schedule.GetWarningTime(1), 19.f);
	TestEqual(TEXT("batch 5 warns at 99 s"), Schedule.GetWarningTime(5), 99.f);

	TestEqual(TEXT("nothing is due before t = 0"), Schedule.GetLatestDueBatch(-0.1f), INDEX_NONE);
	TestEqual(TEXT("batch 0 is due at 0 s"), Schedule.GetLatestDueBatch(0.f), 0);
	TestEqual(TEXT("still batch 0 just before 20 s"), Schedule.GetLatestDueBatch(19.999f), 0);
	TestEqual(TEXT("batch 1 is due at 20 s"), Schedule.GetLatestDueBatch(20.f), 1);
	TestEqual(TEXT("batch 5 is due at 100 s"), Schedule.GetLatestDueBatch(100.f), 5);
	TestEqual(TEXT("no batch beyond the last one"), Schedule.GetLatestDueBatch(1000.f), 5);

	TestEqual(TEXT("batch 0 never warns"), Schedule.GetPendingWarningBatch(0.f, INDEX_NONE), INDEX_NONE);
	TestEqual(TEXT("batch 1 warns at 19 s"), Schedule.GetPendingWarningBatch(19.f, INDEX_NONE), 1);
	TestEqual(TEXT("batch 1 warns only once"), Schedule.GetPendingWarningBatch(19.f, 1), INDEX_NONE);
	TestEqual(TEXT("batch 2 becomes due for a warning next"), Schedule.GetPendingWarningBatch(39.f, 1), 2);

	SlimeFlowMath::FRunSchedule SingleBatch;
	SingleBatch.BatchCount = 1;
	SingleBatch.BatchInterval = 20.f;
	SingleBatch.WarningLead = 1.f;
	TestEqual(TEXT("a single batch never warns"), SingleBatch.GetPendingWarningBatch(100.f, INDEX_NONE), INDEX_NONE);

	SlimeFlowMath::FRunSchedule Invalid;
	TestFalse(TEXT("a zero batch count is invalid"), Invalid.IsValid());
	TestEqual(TEXT("an invalid schedule has nothing due"), Invalid.GetLatestDueBatch(50.f), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeScoringRulesTest,
	"SlimeWar.Flow.ScoringRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeScoringRulesTest::RunTest(const FString& Parameters)
{
	// Design 6.1: a normal kill scores its table value, an aggressive kill scores nothing.
	TestEqual(TEXT("normal mass 1 scores 10"), SlimeFlowMath::ScoreForKill(ETargetKind::Normal, 10), 10);
	TestEqual(TEXT("normal mass 8 scores 80"), SlimeFlowMath::ScoreForKill(ETargetKind::Normal, 80), 80);
	TestEqual(TEXT("aggressive never scores"), SlimeFlowMath::ScoreForKill(ETargetKind::Aggressive, 80), 0);

	// Design 2.5: only a cleared run at or above the target refreshes the best score.
	TestTrue(TEXT("cleared at target refreshes best"),
		SlimeFlowMath::ShouldRefreshBestScore(ERunEndReason::TimeUp, 300, 300));
	TestTrue(TEXT("cleared above target refreshes best"),
		SlimeFlowMath::ShouldRefreshBestScore(ERunEndReason::TimeUp, 1440, 300));
	TestFalse(TEXT("cleared below target does not refresh"),
		SlimeFlowMath::ShouldRefreshBestScore(ERunEndReason::TimeUp, 299, 300));
	TestFalse(TEXT("dying never refreshes, however high the score"),
		SlimeFlowMath::ShouldRefreshBestScore(ERunEndReason::PlayerDied, 9999, 300));
	TestFalse(TEXT("no run end reason does not refresh"),
		SlimeFlowMath::ShouldRefreshBestScore(ERunEndReason::None, 9999, 300));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimePointStateTest,
	"SlimeWar.Flow.PointState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimePointStateTest::RunTest(const FString& Parameters)
{
	// Before the first batch.
	TestEqual(TEXT("before the first batch the point awaits deployment"),
		SlimeFlowMath::EvaluatePointState(false, false, 0), ESpawnPointState::AwaitingDeploy);

	// Supply still running.
	TestEqual(TEXT("while batches are still coming the point is spawning"),
		SlimeFlowMath::EvaluatePointState(false, true, 12), ESpawnPointState::Spawning);

	// Supply done, normals alive.
	TestEqual(TEXT("supply done with live normals is depleted, not cleared"),
		SlimeFlowMath::EvaluatePointState(true, true, 1), ESpawnPointState::DepletedNotCleared);

	// Supply done, no normal left. Aggro is deliberately not part of this call (design 5.3).
	TestEqual(TEXT("supply done with no normal left is cleared"),
		SlimeFlowMath::EvaluatePointState(true, true, 0), ESpawnPointState::Cleared);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeSpawnSupplyTest,
	"SlimeWar.Flow.SpawnSupply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeSpawnSupplyTest::RunTest(const FString& Parameters)
{
	// Design 5.5: 48 normal + 12 aggro per point, 180 entities at peak across three points.
	const int32 NormalPerPoint = SlimeFlowMath::TotalSupply(6, 8);
	const int32 AggroPerPoint = SlimeFlowMath::TotalSupply(6, 2);

	TestEqual(TEXT("48 normal per point"), NormalPerPoint, 48);
	TestEqual(TEXT("12 aggro per point"), AggroPerPoint, 12);
	TestEqual(TEXT("180 entities across three points"), (NormalPerPoint + AggroPerPoint) * 3, 180);

	// The retry window of design 5.2: 2 s at a 0.25 s retry interval, including the attempt at t = 0.
	TestEqual(TEXT("2 s retry window fits 9 attempts"), SlimeFlowMath::GetRetryAttempts(2.f, 0.25f), 9);
	TestEqual(TEXT("a zero window still attempts once"), SlimeFlowMath::GetRetryAttempts(0.f, 0.25f), 1);
	TestEqual(TEXT("a zero interval falls back to one attempt"), SlimeFlowMath::GetRetryAttempts(2.f, 0.f), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
