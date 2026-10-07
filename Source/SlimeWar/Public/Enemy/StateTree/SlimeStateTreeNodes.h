// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Conditions/StateTreeAIConditionBase.h"
#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "Tasks/StateTreeAITask.h"
#include "SlimeStateTreeNodes.generated.h"

/**
 * Phase A enemy AI nodes.
 *
 * Every number these nodes use comes from DA_RunConfig (AI|Normal block) or DT_AggroStats,
 * so tuning never requires opening a StateTree asset (plan rule 5).
 * The StateTree assets only carry structure: which state runs in which order.
 *
 * Instance data is only declared where a node needs to remember a timer or a destination.
 */

/**
 * Nodes that carry no state still MUST declare an instance data type: when a node is created the
 * editor only builds Node->Instance / Node->InstanceObject from GetInstanceDataType(), so
 * returning null makes the StateTree compiler fail with
 * "Malformed task/condition, missing instance value."
 */
USTRUCT()
struct FSlimeSTInstanceDataEmpty
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Normal slime
// ---------------------------------------------------------------------------

/** Waits AISpawnWaitTime (design: 2 s) before the slime starts looking for a partner. */
USTRUCT()
struct FSlimeSTTaskWaitSpawnDelayInstanceData
{
	GENERATED_BODY()

	float RemainingTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Slime: Spawn Delay"))
struct FSlimeSTTaskWaitSpawnDelay : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskWaitSpawnDelayInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** One wander step: a random reachable point inside AIWanderRadius around the activity centre. */
USTRUCT()
struct FSlimeSTTaskWanderStepInstanceData
{
	GENERATED_BODY()

	FVector Destination = FVector::ZeroVector;
	float RemainingTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Slime: Wander Step"))
struct FSlimeSTTaskWanderStep : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskWanderStepInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** Pause between wander steps: random in [AIWanderPauseMin, AIWanderPauseMax] (design 0.5~1.5 s). */
USTRUCT()
struct FSlimeSTTaskWanderPauseInstanceData
{
	GENERATED_BODY()

	float RemainingTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Slime: Wander Pause"))
struct FSlimeSTTaskWanderPause : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskWanderPauseInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * PB-03 target selection: same point, not already pairing, mass sum <= AIFusionMassCap,
 * nearest first.
 *
 * Phase B: the result goes through USlimeFusionComponent::BeginPairing, so the target has to
 * agree (mutual handshake). A slime that accepted someone else's request in the meantime is
 * skipped here and rejected there, which is what keeps "three touching" at exactly one pair.
 */
USTRUCT(meta = (DisplayName = "Slime: Select Fusion Target"))
struct FSlimeSTTaskSelectFusionTarget : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * The fusion state (PB-10 ~ PB-13).
 *
 * Phase A stopped here; Phase B runs the whole approach inside it: walk to the shared meeting
 * point, hold contact for FusionContactTime, then let USlimeFusionComponent resolve and wait
 * out the post-fusion delay. It always returns Running and leaves through the state's condition
 * transition, so the StateTree asset needs no new states - only a "Has Fusion Target" condition
 * transition on Wander/Pause, so a slime that accepted a request while wandering walks into
 * this state on its own.
 */
USTRUCT(meta = (DisplayName = "Slime: Hold Position"))
struct FSlimeSTTaskHoldPosition : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// ---------------------------------------------------------------------------
// Aggressive slime
// ---------------------------------------------------------------------------

/**
 * Tracks the player with pathfinding (the move request follows the moving goal actor itself,
 * so no manual re-path is needed). Succeeds when the player is gone or dead.
 */
USTRUCT(meta = (DisplayName = "Slime: Chase Player"))
struct FSlimeSTTaskChasePlayer : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** Stops moving, locks facing and waits AttackWindupTime (design: 0.5 s). */
USTRUCT()
struct FSlimeSTTaskAttackWindupInstanceData
{
	GENERATED_BODY()

	float RemainingTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Slime: Attack Windup"))
struct FSlimeSTTaskAttackWindup : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskAttackWindupInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/**
 * Re-checks range and line of sight and lands the hit through USlimeCombatSubsystem.
 * Never knocks back, never slows, never damages through a wall.
 */
USTRUCT(meta = (DisplayName = "Slime: Attack Resolve"))
struct FSlimeSTTaskAttackResolve : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

/** Waits AttackRecoverTime (design: 0.35 s) after the hit check. */
USTRUCT()
struct FSlimeSTTaskAttackTimerInstanceData
{
	GENERATED_BODY()

	float RemainingTime = 0.f;
};

USTRUCT(meta = (DisplayName = "Slime: Attack Recover"))
struct FSlimeSTTaskAttackRecover : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskAttackTimerInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * Keeps the gap between two attack *starts* at AttackCooldown (design: >= 1.6 s):
 * duration = max(0, AttackCooldown - AttackWindupTime - AttackRecoverTime).
 */
USTRUCT(meta = (DisplayName = "Slime: Attack Cooldown"))
struct FSlimeSTTaskAttackCooldown : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTTaskAttackTimerInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/** Terminal state: player dead / out of the run. Stops the pawn and stays put. */
USTRUCT(meta = (DisplayName = "Slime: Stop"))
struct FSlimeSTTaskStop : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// ---------------------------------------------------------------------------
// Conditions
// ---------------------------------------------------------------------------

/**
 * True while the slime is inside a fusion (approaching / contacting / cooling down).
 * Tick Invert to test "is free to look for a partner". Backed by USlimeFusionComponent.
 */
USTRUCT(meta = (DisplayName = "Slime: Has Fusion Target"))
struct FSlimeSTCondHasFusionTarget : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/**
	 * Test the opposite. Engine condition nodes expose the same "Invert" property; StateTree has
	 * no global negation, so each condition has to offer it.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** True while a live player pawn exists. Tick Invert to test "player is dead". */
USTRUCT(meta = (DisplayName = "Slime: Player Alive"))
struct FSlimeSTCondPlayerAlive : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

/** In AttackRange and with clear line of sight (cooldown is owned by the cooldown state). */
USTRUCT(meta = (DisplayName = "Slime: Can Attack"))
struct FSlimeSTCondCanAttack : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSlimeSTInstanceDataEmpty;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
