// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_FireCooldown.generated.h"

/**
 * Fire rate gate. Duration comes from SetByCaller "Data.FireCooldown" (1 / fire rate),
 * so the weapon's rate of fire is still pure data.
 */
UCLASS()
class UGE_FireCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_FireCooldown();
};
