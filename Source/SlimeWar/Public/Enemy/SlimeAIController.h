// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SlimeAIController.generated.h"

class USlimeStateTreeAIComponent;
class ASlimeEnemyBase;

/**
 * Runs the enemy StateTree. The tree asset comes from the possessed pawn
 * (ASlimeNormal -> ST_SlimeNormal, ASlimeAggro -> ST_SlimeAggro), no Blueprint needed.
 */
UCLASS()
class ASlimeAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASlimeAIController(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * Per slime type Detour Crowd setup (see the implementation for the exact flags).
	 * Aggro slimes steer around blockers; normal slimes only act as obstacles for others so that
	 * two of them can still walk into each other and fuse.
	 */
	void ConfigureCrowdBehaviour(const ASlimeEnemyBase& Enemy);

	UFUNCTION()
	void HandleEnemyDied(AActor* Enemy);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|AI")
	TObjectPtr<USlimeStateTreeAIComponent> StateTreeAI;
};
