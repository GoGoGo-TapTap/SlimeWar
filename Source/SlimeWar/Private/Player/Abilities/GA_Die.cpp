// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Die.h"
#include "Core/SlimeGameplayTags.h"

UGA_Die::UGA_Die()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Dying owns the dead tag, so every other ability is blocked from that point on.
	ActivationOwnedTags.AddTag(TAG_State_Player_Dead);
}
