// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Die.generated.h"

/**
 * PA-11: the player death sequence.
 *
 * Cancels everything that is still running (an in-flight reload included), applies the death
 * effects on the character (loose State.Player.Dead + movement lock) and reports
 * IBattleDirector::OnPlayerDied. Triggered by code when the mirrored health reaches zero.
 */
UCLASS()
class UGA_Die : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Die();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
