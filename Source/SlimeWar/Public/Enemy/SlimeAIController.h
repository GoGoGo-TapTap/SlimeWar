// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SlimeAIController.generated.h"

class USlimeStateTreeAIComponent;

/**
 * Runs the enemy StateTree. The tree asset comes from the possessed pawn
 * (ASlimeNormal -> ST_SlimeNormal, ASlimeAggro -> ST_SlimeAggro), no Blueprint needed.
 */
UCLASS()
class ASlimeAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASlimeAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	void HandleEnemyDied(AActor* Enemy);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|AI")
	TObjectPtr<USlimeStateTreeAIComponent> StateTreeAI;
};
