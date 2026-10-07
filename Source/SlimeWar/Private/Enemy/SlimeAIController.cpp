// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeAIController.h"

#include "Core/SlimeWarLog.h"
#include "Core/SlimeWarCVars.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeStateTreeAIComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "StateTree.h"

ASlimeAIController::ASlimeAIController(const FObjectInitializer& ObjectInitializer)
	// Replace the base class's default path follower with the Detour Crowd one, so MoveTo requests
	// can steer around other agents. Which agents actually avoid which is decided per pawn in
	// ConfigureCrowdBehaviour.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	StateTreeAI = CreateDefaultSubobject<USlimeStateTreeAIComponent>(TEXT("StateTreeAI"));

	// Possession happens after the component's BeginPlay, so the asset is assigned by us
	// and the logic is started explicitly in OnPossess.
	StateTreeAI->SetStartLogicAutomatically(false);

	BrainComponent = StateTreeAI;
}

void ASlimeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ASlimeEnemyBase* Enemy = Cast<ASlimeEnemyBase>(InPawn);
	if (!Enemy || !StateTreeAI)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("ASlimeAIController::OnPossess: %s is not an ASlimeEnemyBase."),
			*GetNameSafe(InPawn));
		return;
	}

	// Before anything starts moving: SetCrowdSimulationState refuses to run mid move.
	ConfigureCrowdBehaviour(*Enemy);

	Enemy->OnEnemyDied.AddDynamic(this, &ASlimeAIController::HandleEnemyDied);

	UStateTree* Tree = Enemy->GetStateTreeAsset().LoadSynchronous();
	if (!Tree)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("ASlimeAIController::OnPossess: %s has no StateTree asset. Create the StateTree in the editor ")
			TEXT("(see Docs/PhaseA)."), *GetNameSafe(Enemy));
		return;
	}

	StateTreeAI->SetStateTreeAsset(Tree);
	StateTreeAI->StartLogic();
}

void ASlimeAIController::ConfigureCrowdBehaviour(const ASlimeEnemyBase& Enemy)
{
	UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!Crowd)
	{
		return;
	}

	if (Enemy.IsAggressive())
	{
		// Design 4.3: "遇障碍绕行". Obstacle avoidance is what makes the chaser pick its way around
		// slimes and the player instead of grinding into them. Separation stays off (engine default)
		// so the final approach to attack range is not pushed away.
		Crowd->SetCrowdSimulationState(ECrowdSimulationState::Enabled);
		Crowd->SetCrowdObstacleAvoidance(true);
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Good);

		if (SlimeCVars::DebugCombatLog != 0)
		{
			UE_LOG(LogSlimeWar, Log, TEXT("[%s] crowd: enabled (avoidance on)."), *GetNameSafe(&Enemy));
		}
		return;
	}

	// Normal slimes MUST keep walking straight into each other: the fusion handshake needs real
	// overlap, so any avoidance here would make a pair drift apart and never touch.
	//
	// ECrowdSimulationState::ObstacleOnly reads right, but in UE 5.5 the enum is only used as a
	// registration flag (see CrowdFollowingComponent.cpp - "ObstacleOnly" is never read anywhere
	// else), so the agent stays registered and the actual switches have to be cleared by hand:
	//   - obstacle avoidance + separation -> the slime itself never steers around anyone
	//   - path offset / visibility + topology optimisation -> the walk stays on the plain
	//     path following result, so both partners still reach the exact same meeting point
	// Being registered is what lets the aggro slimes' avoidance see them as obstacles.
	Crowd->SetCrowdSimulationState(ECrowdSimulationState::ObstacleOnly);
	Crowd->SetCrowdObstacleAvoidance(false);
	Crowd->SetCrowdSeparation(false);
	Crowd->SetCrowdAnticipateTurns(false);
	Crowd->SetCrowdPathOffset(false);
	Crowd->SetCrowdOptimizeVisibility(false);
	Crowd->SetCrowdOptimizeTopology(false);

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] crowd: obstacle only (avoidance off)."), *GetNameSafe(&Enemy));
	}
}

void ASlimeAIController::OnUnPossess()
{
	if (StateTreeAI && StateTreeAI->IsRunning())
	{
		StateTreeAI->StopLogic(TEXT("UnPossessed"));
	}

	Super::OnUnPossess();
}

void ASlimeAIController::HandleEnemyDied(AActor* /*Enemy*/)
{
	if (StateTreeAI && StateTreeAI->IsRunning())
	{
		StateTreeAI->StopLogic(TEXT("EnemyDied"));
	}

	StopMovement();
}
