// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/StateTree/SlimeStateTreeNodes.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "CollisionQueryParams.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeFusionComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AAIController* GetSlimeController(FStateTreeExecutionContext& Context)
	{
		if (UBrainComponent* Brain = Cast<UBrainComponent>(Context.GetOwner()))
		{
			return Brain->GetAIOwner();
		}

		return Cast<AAIController>(Context.GetOwner());
	}

	ASlimeEnemyBase* GetSlimeEnemy(FStateTreeExecutionContext& Context)
	{
		const AAIController* Controller = GetSlimeController(Context);
		return Controller ? Cast<ASlimeEnemyBase>(Controller->GetPawn()) : nullptr;
	}

	UStatTableProvider* GetSlimeProvider(FStateTreeExecutionContext& Context)
	{
		UWorld* World = Context.GetWorld();
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	}

	const USlimeRunConfig* GetSlimeConfig(FStateTreeExecutionContext& Context)
	{
		const UStatTableProvider* Provider = GetSlimeProvider(Context);
		return Provider ? Provider->GetRunConfig() : nullptr;
	}

	bool GetSlimeAggroRow(FStateTreeExecutionContext& Context, FSlimeAggroStatRow& Out)
	{
		UStatTableProvider* Provider = GetSlimeProvider(Context);
		return Provider && Provider->GetAggroStat(Out);
	}

	APawn* GetPlayerPawnFor(FStateTreeExecutionContext& Context)
	{
		return UGameplayStatics::GetPlayerPawn(Context.GetWorld(), 0);
	}

	bool IsAlive(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		const USlimeHealthComponent* Health = Actor->FindComponentByClass<USlimeHealthComponent>();
		return Health && !Health->IsDead();
	}

	bool HasLineOfSight(const UWorld* World, const AActor* From, const AActor* To)
	{
		if (!World || !From || !To)
		{
			return false;
		}

		FCollisionQueryParams Params(FName(TEXT("SlimeLineOfSight")), false, From);
		Params.AddIgnoredActor(To);

		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			Hit, From->GetActorLocation(), To->GetActorLocation(), ECC_Visibility, Params);

		return !bBlocked;
	}

	void SetStateTag(const ASlimeEnemyBase* Enemy, const FGameplayTag& Tag, bool bEnabled)
	{
		if (!Enemy)
		{
			return;
		}

		if (USlimeStateComponent* State = Enemy->GetStateComponent())
		{
			if (bEnabled)
			{
				State->AddStateTag(Tag);
			}
			else
			{
				State->RemoveStateTag(Tag);
			}
		}
	}

	/**
	 * Walks this slime to the shared meeting point.
	 *
	 * Two details matter here (both were wrong in the first Phase B cut and made untroubled pairs
	 * cancel with "could not reach the partner"):
	 *  - FAIMoveRequest defaults to a reach test that adds the agent radius (and the goal radius
	 *    for actor goals). With a 10 cm acceptance radius that becomes ~50-60 cm - already half
	 *    the gap between two slimes standing 1 m apart - so AAIController::MoveTo returns
	 *    "already at goal", nobody moves and the pair times out with a constant gap and an Idle
	 *    move status. The fusion handshake has to measure centre to centre.
	 *  - If the shared point cannot be pathed to (off the navmesh, blocked), fall back to walking
	 *    straight at the partner instead of standing still until the timeout.
	 */
	EPathFollowingRequestResult::Type RequestFusionMove(AAIController& Controller, USlimeFusionComponent& Fusion)
	{
		const float AcceptanceRadius = Fusion.GetApproachAcceptanceRadius();

		const auto ConfigureReachTest = [AcceptanceRadius](FAIMoveRequest& MoveRequest)
		{
			MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
			MoveRequest.SetReachTestIncludesAgentRadius(false);
			MoveRequest.SetReachTestIncludesGoalRadius(false);
			MoveRequest.SetUsePathfinding(true);
			MoveRequest.SetAllowPartialPath(true);
		};

		// Primary: the shared open space point, so neither slime paths into a wall.
		const FVector MeetingPoint = Fusion.GetMeetingPoint();
		if (!MeetingPoint.IsNearlyZero())
		{
			FAIMoveRequest MoveRequest(MeetingPoint);
			ConfigureReachTest(MoveRequest);
			MoveRequest.SetProjectGoalLocation(true);

			const FPathFollowingRequestResult MoveResult = Controller.MoveTo(MoveRequest);
			if (MoveResult.Code != EPathFollowingRequestResult::Failed)
			{
				return MoveResult.Code;
			}
		}

		// Fallback: walk straight at the partner (the shared point may be off the navmesh).
		if (ASlimeEnemyBase* Partner = Fusion.GetPartner())
		{
			FAIMoveRequest MoveRequest(Partner);
			ConfigureReachTest(MoveRequest);
			return Controller.MoveTo(MoveRequest).Code;
		}

		return EPathFollowingRequestResult::Failed;
	}
}

// ---------------------------------------------------------------------------
// Normal slime
// ---------------------------------------------------------------------------

EStateTreeRunStatus FSlimeSTTaskWaitSpawnDelay::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const USlimeRunConfig* Config = GetSlimeConfig(Context);
	InstanceData.RemainingTime = Config ? Config->AISpawnWaitTime : 0.f;

	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Normal_Fusing, false);
	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Normal_Idle, true);

	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskWaitSpawnDelay::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskWanderStep::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	AAIController* Controller = GetSlimeController(Context);
	const USlimeRunConfig* Config = GetSlimeConfig(Context);

	// Do not report "idle" while a fusion request was already accepted: the condition transition
	// is about to move this slime into the fusion state, so flipping the tag here would flicker.
	const bool bAlreadyFusing = Enemy && Enemy->GetFusionComponent() && Enemy->GetFusionComponent()->IsEngaged();
	if (!bAlreadyFusing)
	{
		SetStateTag(Enemy, TAG_State_Enemy_Normal_Fusing, false);
		SetStateTag(Enemy, TAG_State_Enemy_Normal_Idle, true);
	}

	if (!Enemy || !Controller || !Config)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.RemainingTime = Config->AIApproachTimeout;
	InstanceData.Destination = Enemy->GetActivityCenter();

	const float Radius = Config->AIWanderRadius;

	if (!Enemy->IsInsideActivityArea())
	{
		// Drifted outside its point (a long fusion walk, a shove, a partial path): walk back to the
		// nearest point inside the area instead of wandering further away. Design 5.1: normal
		// targets only ever act inside their own point.
		InstanceData.Destination = Enemy->ClampToActivityArea(Enemy->GetActorLocation());
	}
	else if (Radius > 0.f && Context.GetWorld())
	{
		FVector Candidate = FVector::ZeroVector;
		if (UNavigationSystemV1::K2_GetRandomReachablePointInRadius(
			Context.GetWorld(), Enemy->GetActivityCenter(), Candidate, Radius))
		{
			// Clamp as well: the random point is picked around the centre, but a slime standing at
			// the edge could still be asked to step outside.
			InstanceData.Destination = Enemy->ClampToActivityArea(Candidate);
		}
	}

	Controller->MoveToLocation(InstanceData.Destination);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskWanderStep::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = GetSlimeController(Context);

	if (!Controller)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.RemainingTime -= DeltaTime;

	if (InstanceData.RemainingTime <= 0.f)
	{
		Controller->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}

	if (Controller->GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FSlimeSTTaskWanderStep::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->StopMovement();
	}
}

EStateTreeRunStatus FSlimeSTTaskWanderPause::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const USlimeRunConfig* Config = GetSlimeConfig(Context);
	const float Min = Config ? Config->AIWanderPauseMin : 0.f;
	const float Max = Config ? FMath::Max(Config->AIWanderPauseMax, Min) : 0.f;

	InstanceData.RemainingTime = FMath::FRandRange(Min, Max);

	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	const bool bAlreadyFusing = Enemy && Enemy->GetFusionComponent() && Enemy->GetFusionComponent()->IsEngaged();
	if (!bAlreadyFusing)
	{
		SetStateTag(Enemy, TAG_State_Enemy_Normal_Fusing, false);
		SetStateTag(Enemy, TAG_State_Enemy_Normal_Idle, true);
	}

	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskWanderPause::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskSelectFusionTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	const USlimeRunConfig* Config = GetSlimeConfig(Context);

	if (!Enemy || !Config || !Context.GetWorld())
	{
		return EStateTreeRunStatus::Failed;
	}

	USlimeFusionComponent* Fusion = Enemy->GetFusionComponent();
	if (!Fusion)
	{
		// Aggressive slimes never reach this task; a normal slime without the component is a bug.
		return EStateTreeRunStatus::Failed;
	}

	// The request this slime accepted while it was still wandering already reserved it. Its own
	// state chain only has to reach "Hold" now; the fusion state itself is owned by the component.
	if (Fusion->IsEngaged())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const int32 MassCap = Config->AIFusionMassCap;
	if (MassCap > 0 && Enemy->GetMass() >= MassCap)
	{
		// Mass 8 is locked and never fuses again (design 4.4).
		SetStateTag(Enemy, TAG_State_Enemy_Normal_MassLocked, true);
		return EStateTreeRunStatus::Failed;
	}

	SetStateTag(Enemy, TAG_State_Enemy_Normal_MassLocked, false);

	ASlimeEnemyBase* BestTarget = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASlimeEnemyBase> It(Context.GetWorld()); It; ++It)
	{
		ASlimeEnemyBase* Other = *It;
		if (!Other || Other == Enemy || Other->IsAggressive())
		{
			continue;
		}

		if (Other->GetPointId() != Enemy->GetPointId())
		{
			continue;
		}

		// Already pairing (or waiting out a fusion cooldown).
		const USlimeFusionComponent* OtherFusion = Other->GetFusionComponent();
		if (!OtherFusion || OtherFusion->IsEngaged())
		{
			continue;
		}

		if (!IsAlive(Other))
		{
			continue;
		}

		if (MassCap > 0 && Enemy->GetMass() + Other->GetMass() > MassCap)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestTarget = Other;
		}
	}

	if (!BestTarget)
	{
		return EStateTreeRunStatus::Failed;
	}

	// PB-09: the pairwise handshake replaces the Phase A "I picked you" write. The target has to
	// agree; if it accepted someone else a moment earlier this fails and we try again later.
	if (!Fusion->BeginPairing(BestTarget))
	{
		if (SlimeCVars::DebugCombatLog != 0)
		{
			UE_LOG(LogSlimeWar, Log, TEXT("[%s] fusion request to %s was rejected (already engaged)."),
				*GetNameSafe(Enemy), *GetNameSafe(BestTarget));
		}

		return EStateTreeRunStatus::Failed;
	}

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] fusion pairing accepted by %s (mass %d + %d, cap %d)"),
			*GetNameSafe(Enemy), *GetNameSafe(BestTarget), Enemy->GetMass(), BestTarget->GetMass(), MassCap);
	}

	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskHoldPosition::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	AAIController* Controller = GetSlimeController(Context);
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	USlimeFusionComponent* Fusion = Enemy ? Enemy->GetFusionComponent() : nullptr;

	// Phase B: this state is the fusion approach. The pair is already accepted here; all that is
	// left is walking to the shared meeting point, holding contact for FusionContactTime and then
	// letting the component resolve (PB-10 ~ PB-13). The state always reports Running and leaves
	// through its condition transition, exactly like in Phase A - a task that "completes" would
	// have no completion transition and would make the engine jump back to the root state.
	if (!Fusion || !Fusion->IsEngaged())
	{
		if (Controller)
		{
			Controller->StopMovement();
		}

		return EStateTreeRunStatus::Running;
	}

	if (Controller)
	{
		const EPathFollowingRequestResult::Type Result = RequestFusionMove(*Controller, *Fusion);
		if (Result == EPathFollowingRequestResult::Failed && SlimeCVars::DebugCombatLog != 0)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("[%s] fusion: could not start a move towards the partner (navmesh missing or point unreachable)."),
				*GetNameSafe(Enemy));
		}
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskHoldPosition::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	AAIController* Controller = GetSlimeController(Context);
	USlimeFusionComponent* Fusion = Enemy ? Enemy->GetFusionComponent() : nullptr;

	if (!Fusion || !Fusion->IsEngaged())
	{
		if (Controller)
		{
			Controller->StopMovement();
		}

		return EStateTreeRunStatus::Running;
	}

	Fusion->AdvanceApproach(DeltaTime);

	if (Controller)
	{
		if (Fusion->GetState() == ESlimeFusionState::Approaching)
		{
			// Re-issue the move if the pawn stopped short (blocked, path ended, ...).
			if (Controller->GetMoveStatus() != EPathFollowingStatus::Moving)
			{
				const float AcceptanceRadius = Fusion->GetApproachAcceptanceRadius();
				if (FVector::Dist(Enemy->GetActorLocation(), Fusion->GetMeetingPoint()) > AcceptanceRadius)
				{
					RequestFusionMove(*Controller, *Fusion);
				}
			}
		}
		else
		{
			// Contacting or cooling down: stand still.
			Controller->StopMovement();
		}
	}

	return EStateTreeRunStatus::Running;
}

void FSlimeSTTaskHoldPosition::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->StopMovement();
	}

	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	if (Enemy)
	{
		// Defensive: never leave the state with a half registered pair (collision ignore, tags).
		if (USlimeFusionComponent* Fusion = Enemy->GetFusionComponent())
		{
			if (Fusion->IsPaired())
			{
				Fusion->CancelPairing(ESlimeFusionCancelReason::PartnerLost);
			}
		}

		Enemy->SetFusionTarget(nullptr);
	}

	SetStateTag(Enemy, TAG_State_Enemy_Normal_Fusing, false);
	SetStateTag(Enemy, TAG_State_Enemy_Normal_Idle, true);
}

// ---------------------------------------------------------------------------
// Aggressive slime
// ---------------------------------------------------------------------------

EStateTreeRunStatus FSlimeSTTaskChasePlayer::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	AAIController* Controller = GetSlimeController(Context);
	APawn* Player = GetPlayerPawnFor(Context);

	SetStateTag(Enemy, TAG_State_Enemy_Aggro_WindingUp, false);
	SetStateTag(Enemy, TAG_State_Enemy_Aggro_Recovering, false);
	SetStateTag(Enemy, TAG_State_Enemy_Aggro_Cooling, false);
	SetStateTag(Enemy, TAG_State_Enemy_Aggro_Chasing, true);

	// Refresh in case the player pawn was respawned since this slime was spawned.
	if (Enemy)
	{
		Enemy->IgnorePlayerForMovement();
	}

	if (!Controller || !Enemy || !IsAlive(Player))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// The move request tracks the moving goal actor, so the chase keeps following the player.
	Controller->MoveToActor(Player);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskChasePlayer::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	APawn* Player = GetPlayerPawnFor(Context);
	if (!IsAlive(Player))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	AAIController* Controller = GetSlimeController(Context);
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);

	// Self healing repath. When the path following gives up (blocked by another slime, stale path)
	// the move goes Idle while this task keeps Running, which used to leave the slime grinding in
	// place ("is stuck and failed to move!"). Design 4.6.4 asks for "wait and try again", so
	// re-issue the chase whenever nothing is moving and the player is still out of attack range.
	if (Controller && Enemy && Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		FSlimeAggroStatRow Row;
		const float AttackRange = GetSlimeAggroRow(Context, Row) ? Row.AttackRange : 0.f;
		const bool bOutOfAttackRange =
			FVector::Dist(Enemy->GetActorLocation(), Player->GetActorLocation()) > AttackRange;

		if (bOutOfAttackRange)
		{
			Controller->MoveToActor(Player);
		}
	}

	return EStateTreeRunStatus::Running;
}

void FSlimeSTTaskChasePlayer::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->StopMovement();
	}
}

EStateTreeRunStatus FSlimeSTTaskAttackWindup::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	AAIController* Controller = GetSlimeController(Context);
	APawn* Player = GetPlayerPawnFor(Context);

	SetStateTag(Enemy, TAG_State_Enemy_Aggro_Chasing, false);
	SetStateTag(Enemy, TAG_State_Enemy_Aggro_WindingUp, true);

	if (Controller)
	{
		Controller->StopMovement();
	}

	// Direction is locked when the wind-up starts (design 4.3).
	if (Controller && Player)
	{
		Controller->SetFocus(Player);
	}

	FSlimeAggroStatRow Row;
	InstanceData.RemainingTime = GetSlimeAggroRow(Context, Row) ? Row.AttackWindupTime : 0.f;

	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskAttackWindup::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FSlimeSTTaskAttackWindup::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

EStateTreeRunStatus FSlimeSTTaskAttackResolve::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	APawn* Player = GetPlayerPawnFor(Context);

	SetStateTag(Enemy, TAG_State_Enemy_Aggro_WindingUp, false);

	if (!Enemy || !IsAlive(Player))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	FSlimeAggroStatRow Row;
	if (!GetSlimeAggroRow(Context, Row) || Row.AttackRange <= 0.f)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// The player may have walked out of range or behind a wall during the wind-up: that is a miss.
	const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Player->GetActorLocation());
	if (DistSq > FMath::Square(Row.AttackRange))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (!HasLineOfSight(Context.GetWorld(), Enemy, Player))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// Everything goes through the one damage entry point: no knockback, no slow, no wall damage.
	if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(Enemy))
	{
		Combat->ApplyDamageTo(Player, Row.AttackDamage, Enemy);
	}

	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskAttackRecover::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Aggro_Recovering, true);

	FSlimeAggroStatRow Row;
	InstanceData.RemainingTime = GetSlimeAggroRow(Context, Row) ? Row.AttackRecoverTime : 0.f;

	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskAttackRecover::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskAttackCooldown::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FSlimeAggroStatRow Row;
	const bool bHasRow = GetSlimeAggroRow(Context, Row);

	// Two attack starts must be at least AttackCooldown apart; the wind-up and recovery
	// already consume part of that window, so only the remainder is waited out here.
	InstanceData.RemainingTime = bHasRow
		? FMath::Max(0.f, Row.AttackCooldown - Row.AttackWindupTime - Row.AttackRecoverTime)
		: 0.f;

	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Aggro_Recovering, false);
	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Aggro_Cooling, true);

	return InstanceData.RemainingTime > 0.f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskAttackCooldown::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.RemainingTime -= DeltaTime;

	return InstanceData.RemainingTime <= 0.f ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSlimeSTTaskStop::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->StopMovement();
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context))
	{
		Enemy->ClearAllStateTags();
		Enemy->SetFusionTarget(nullptr);
	}

	return EStateTreeRunStatus::Running;
}

// ---------------------------------------------------------------------------
// Conditions
// ---------------------------------------------------------------------------

bool FSlimeSTCondHasFusionTarget::TestCondition(FStateTreeExecutionContext& Context) const
{
	const ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	if (!Enemy)
	{
		return false ^ bInvert;
	}

	// Phase B: ask the fusion component instead of the mirrored pawn field. "Has a fusion target"
	// now means approaching, contacting or cooling down, so the Hold state also covers the
	// post-fusion 1 s wait (design 4.4) without an extra StateTree state.
	const USlimeFusionComponent* Fusion = Enemy->GetFusionComponent();
	const bool bEngaged = Fusion != nullptr && Fusion->IsEngaged();
	return bEngaged ^ bInvert;
}

bool FSlimeSTCondPlayerAlive::TestCondition(FStateTreeExecutionContext& Context) const
{
	return IsAlive(GetPlayerPawnFor(Context)) ^ bInvert;
}

bool FSlimeSTCondCanAttack::TestCondition(FStateTreeExecutionContext& Context) const
{
	const ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	const APawn* Player = GetPlayerPawnFor(Context);
	if (!Enemy || !IsAlive(Player))
	{
		return false ^ bInvert;
	}

	FSlimeAggroStatRow Row;
	if (!GetSlimeAggroRow(Context, Row) || Row.AttackRange <= 0.f)
	{
		return false ^ bInvert;
	}

	const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Player->GetActorLocation());
	if (DistSq > FMath::Square(Row.AttackRange))
	{
		return false ^ bInvert;
	}

	return HasLineOfSight(Context.GetWorld(), Enemy, Player) ^ bInvert;
}
