// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Reload.generated.h"

/**
 * Reload ability skeleton. Owning State.Weapon.Reloading blocks firing. The actual refill and
 * the AbilityTask_WaitDelay come in Phase A (PA-06). Move / aim stay allowed while reloading.
 */
UCLASS()
class UGA_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Reload();
};
