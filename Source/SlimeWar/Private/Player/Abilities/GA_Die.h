// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Die.generated.h"

/**
 * Death ability skeleton. Adds State.Player.Dead, cancels every other ability and reports
 * IBattleDirector::OnPlayerDied. Implemented in Phase B (PA-11).
 */
UCLASS()
class UGA_Die : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Die();
};
