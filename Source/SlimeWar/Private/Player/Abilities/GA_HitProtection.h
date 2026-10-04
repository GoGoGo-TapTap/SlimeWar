// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitProtection.generated.h"

/**
 * Hit protection ability skeleton. Applies GE_Invulnerable for the duration taken from
 * DA_RunConfig (design: 0.6s). Implemented in Phase A (PA-07).
 */
UCLASS()
class UGA_HitProtection : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HitProtection();
};
