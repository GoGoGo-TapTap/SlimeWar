// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Fire.generated.h"

/**
 * Fire ability skeleton. Phase 0 only declares the activation gates; the line trace,
 * muzzle occlusion check and damage routing arrive in Phase A (PA-04 / PA-05).
 */
UCLASS()
class UGA_Fire : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Fire();
};
