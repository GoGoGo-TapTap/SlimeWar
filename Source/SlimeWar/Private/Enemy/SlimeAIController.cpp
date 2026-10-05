// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeAIController.h"

#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeStateTreeAIComponent.h"
#include "StateTree.h"

ASlimeAIController::ASlimeAIController()
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
