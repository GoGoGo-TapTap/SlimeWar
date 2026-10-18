// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SlimeEcologyAIController.generated.h"

/** Possession/lifecycle only. Movement and decisions live in the pawn components. */
UCLASS()
class SLIMEWAR_API ASlimeEcologyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASlimeEcologyAIController();
};
