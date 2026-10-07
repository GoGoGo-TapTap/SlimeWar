// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Reload.generated.h"

/**
 * PA-06: refills the magazine after ReloadDuration.
 * Owning State.Weapon.Reloading is what blocks GA_Fire, while movement and aiming stay free.
 */
UCLASS()
class UGA_Reload : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Reload();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void HandleReloadFinished();
};
