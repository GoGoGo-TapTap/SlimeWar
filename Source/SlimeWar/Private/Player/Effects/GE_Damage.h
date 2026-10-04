// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Damage.generated.h"

/**
 * Instant damage to the player Health attribute.
 *
 * Structure only (rule 5): the amount is NOT stored here, it is injected at apply time through
 * SetByCaller "Data.Damage" by USlimeCombatSubsystem. Because the modifier is additive the
 * caller passes a negative value.
 */
UCLASS()
class UGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Damage();
};
