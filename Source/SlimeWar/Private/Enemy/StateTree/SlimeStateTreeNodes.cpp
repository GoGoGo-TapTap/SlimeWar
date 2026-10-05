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

	SetStateTag(Enemy, TAG_State_Enemy_Normal_Fusing, false);
	SetStateTag(Enemy, TAG_State_Enemy_Normal_Idle, true);

	if (!Enemy || !Controller || !Config)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.RemainingTime = Config->AIApproachTimeout;
	InstanceData.Destination = Enemy->GetActivityCenter();

	const float Radius = Config->AIWanderRadius;
	if (Radius > 0.f && Context.GetWorld())
	{
		FVector Candidate = FVector::ZeroVector;
		if (UNavigationSystemV1::K2_GetRandomReachablePointInRadius(
			Context.GetWorld(), Enemy->GetActivityCenter(), Candidate, Radius))
		{
			InstanceData.Destination = Candidate;
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

	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Normal_Fusing, false);
	SetStateTag(GetSlimeEnemy(Context), TAG_State_Enemy_Normal_Idle, true);

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

		// Already pairing with someone else.
		if (Other->GetFusionTarget() != nullptr)
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

	Enemy->SetFusionTarget(BestTarget);
	SetStateTag(Enemy, TAG_State_Enemy_Normal_Idle, false);
	SetStateTag(Enemy, TAG_State_Enemy_Normal_Fusing, true);

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] fusion target selected: %s (mass %d + %d, cap %d)"),
			*GetNameSafe(Enemy), *GetNameSafe(BestTarget), Enemy->GetMass(), BestTarget->GetMass(), MassCap);
	}

	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FSlimeSTTaskHoldPosition::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	if (AAIController* Controller = GetSlimeController(Context))
	{
		Controller->StopMovement();
	}

	// Phase A ends here: the partner is stored and tagged. Phase B adds the approach,
	// the handshake and the 0.4 s contact check (PB-09 ~ PB-11).
	return EStateTreeRunStatus::Running;
}

void FSlimeSTTaskHoldPosition::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	ASlimeEnemyBase* Enemy = GetSlimeEnemy(Context);
	if (Enemy)
	{
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
	return IsAlive(GetPlayerPawnFor(Context)) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
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

	ASlimeEnemyBase* Target = Enemy->GetFusionTarget();
	return (Target != nullptr && IsAlive(Target)) ^ bInvert;
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
