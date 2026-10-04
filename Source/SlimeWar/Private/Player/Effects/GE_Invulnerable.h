// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Invulnerable.generated.h"

/**
 * Hit protection window. Duration comes from SetByCaller "Data.HitProtectionDuration".
 *
 * TODO(Phase A, PA-07 / PA-16): grant State.Player.Invulnerable while active, and make
 * USlimeCombatSubsystem skip damage while that tag is present.
 */
UCLASS()
class UGE_Invulnerable : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_Invulnerable();
};
