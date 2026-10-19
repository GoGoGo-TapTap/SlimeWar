// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Flow/SlimeFlowMath.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Pure-logic coverage for the run flow. These tests touch no UObject and no world: scoring and
 * phase transitions are the rules that must not silently drift.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeScoringRulesTest,
	"SlimeWar.Flow.ScoringRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeScoringRulesTest::RunTest(const FString& Parameters)
{
	FSlimeKillContext Context;
	Context.Mass = 8;
	Context.bKilledByPlayer = true;
	TestEqual(TEXT("mass 8 body scores 68"), SlimeFlowMath::EcologyKillScore(Context), 68);

	Context.Cargo = 2;
	TestEqual(TEXT("mass 8 with 2 cargo scores 80"), SlimeFlowMath::EcologyKillScore(Context), 80);

	Context.bHostileCore = true;
	TestEqual(TEXT("hostile core awards half"), SlimeFlowMath::EcologyKillScore(Context), 40);

	Context.bHostileCore = false;
	Context.bKilledByPlayer = false;
	TestEqual(TEXT("NPC kills award no player score"), SlimeFlowMath::EcologyKillScore(Context), 0);

	FSlimeKillContext CreditContext;
	CreditContext.Mass = 1;
	CreditContext.bKilledByPlayer = true;
	CreditContext.BodyScoreCredit = 30;
	TestEqual(TEXT("inherited body credit is preserved"), SlimeFlowMath::EcologyKillScore(CreditContext), 30);

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

#endif // WITH_DEV_AUTOMATION_TESTS
