// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Fire.generated.h"

/**
 * One activation = one shot (PA-04 / PA-05).
 *
 * The camera->aim-point trace and the muzzle occlusion re-check live in
 * ASlimeWarCharacter::PerformShot; damage always goes through USlimeCombatSubsystem.
 * Rate of fire is the native ability cooldown: CooldownGameplayEffectClass applies
 * GE_FireCooldown, and ApplyCooldown injects 1/FireRate through SetByCaller.
 * Hold-to-fire is driven by the character re-triggering this ability while the
 * cooldown tag is absent.
 */
UCLASS()
class UGA_Fire : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Fire();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
