// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Flow/SlimeFlowMath.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Pure-logic coverage for Phase D (CP-4).
 *
 * Like the Phase C tests these touch no UObject and no world. The lifecycle order, the "passed"
 * rule and the drop point bounds are exactly the parts that would otherwise drift between the run
 * subsystem, the settlement screen and the retry path.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeRunPhaseTest,
	"SlimeWar.Flow.RunPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeRunPhaseTest::RunTest(const FString& Parameters)
{
	using ESlimeRunState::Idle;
	using ESlimeRunState::Deploying;
	using ESlimeRunState::Running;
	using ESlimeRunState::Result;
	using ESlimeRunState::Ended;

	// The happy path, in lifecycle order.
	TestTrue(TEXT("Idle -> Deploying"), SlimeFlowMath::IsValidRunPhaseTransition(Idle, Deploying));
	TestTrue(TEXT("Deploying -> Running"), SlimeFlowMath::IsValidRunPhaseTransition(Deploying, Running));
	TestTrue(TEXT("Running -> Result"), SlimeFlowMath::IsValidRunPhaseTransition(Running, Result));
	TestTrue(TEXT("Result -> Ended"), SlimeFlowMath::IsValidRunPhaseTransition(Result, Ended));

	// SlimeRunStart skips the cinematic on purpose (CP-3 regression).
	TestTrue(TEXT("Idle -> Running (cheat path)"), SlimeFlowMath::IsValidRunPhaseTransition(Idle, Running));

	// Nothing ever moves backwards, and nothing skips the settlement.
	TestFalse(TEXT("Running -> Idle"), SlimeFlowMath::IsValidRunPhaseTransition(Running, Idle));
	TestFalse(TEXT("Running -> Ended (must pass through Result)"),
		SlimeFlowMath::IsValidRunPhaseTransition(Running, Ended));
	TestFalse(TEXT("Deploying -> Result"), SlimeFlowMath::IsValidRunPhaseTransition(Deploying, Result));
	TestFalse(TEXT("Ended is terminal"), SlimeFlowMath::IsValidRunPhaseTransition(Ended, Idle));

	// Every state may stay put; SetRunState filters those before it ever asks.
	for (const ESlimeRunState State : { Idle, Deploying, Running, Result, Ended })
	{
		TestFalse(TEXT("a phase does not transition to itself"),
			SlimeFlowMath::IsValidRunPhaseTransition(State, State));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeRunResultTest,
	"SlimeWar.Flow.RunResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeRunResultTest::RunTest(const FString& Parameters)
{
	using ERunEndReason::None;
	using ERunEndReason::TimeUp;
	using ERunEndReason::PlayerDied;

	// Design 2.5: only running the clock out with the minimum score met counts as passed.
	TestTrue(TEXT("time up and on target passes"),
		SlimeFlowMath::ComputeRunPassed(TimeUp, 300, 300));
	TestTrue(TEXT("time up above target passes"),
		SlimeFlowMath::ComputeRunPassed(TimeUp, 640, 300));
	TestFalse(TEXT("time up below target fails"),
		SlimeFlowMath::ComputeRunPassed(TimeUp, 299, 300));

	// Dying is a failure even with a high score, and the run cannot end before it is over.
	TestFalse(TEXT("death fails even above target"),
		SlimeFlowMath::ComputeRunPassed(PlayerDied, 500, 300));
	TestFalse(TEXT("no end reason is not passed"),
		SlimeFlowMath::ComputeRunPassed(None, 500, 300));

	// The best score uses the same rule, which is why they must agree.
	for (const int32 Score : { 0, 299, 300, 1000 })
	{
		TestEqual(
			FString::Printf(TEXT("best score rule matches passed rule at %d"), Score),
			SlimeFlowMath::ShouldRefreshBestScore(TimeUp, Score, 300),
			SlimeFlowMath::ComputeRunPassed(TimeUp, Score, 300));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeDropPointSelectionTest,
	"SlimeWar.Flow.DropPointSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeDropPointSelectionTest::RunTest(const FString& Parameters)
{
	// Two drop points (D1 / D2) is the current design; the index is what DA_SpawnLayout stores.
	TestFalse(TEXT("no drop points configured"), SlimeFlowMath::IsValidDropIndex(0, 0));
	TestTrue(TEXT("first drop point"), SlimeFlowMath::IsValidDropIndex(0, 2));
	TestTrue(TEXT("second drop point"), SlimeFlowMath::IsValidDropIndex(1, 2));
	TestFalse(TEXT("one past the end"), SlimeFlowMath::IsValidDropIndex(2, 2));
	TestFalse(TEXT("negative index"), SlimeFlowMath::IsValidDropIndex(-1, 2));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
