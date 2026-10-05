// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeAIComponent.h"
#include "SlimeStateTreeAIComponent.generated.h"

class UStateTree;

/**
 * UStateTreeComponent keeps StateTreeRef protected, so C++ (which assigns the asset at
 * possess time instead of in a Blueprint) needs this one accessor.
 */
UCLASS()
class USlimeStateTreeAIComponent : public UStateTreeAIComponent
{
	GENERATED_BODY()

public:
	void SetStateTreeAsset(UStateTree* InStateTree);
};
