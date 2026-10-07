// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitProtection.generated.h"

/**
 * PA-07: opens the post-hit protection window by applying GE_Invulnerable with
 * Data.HitProtectionDuration (design 0.6 s). Triggered by code when the mirrored
 * health drops, never by an input.
 */
UCLASS()
class UGA_HitProtection : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HitProtection();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
