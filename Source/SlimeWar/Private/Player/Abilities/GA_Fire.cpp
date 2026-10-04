// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Fire.h"
#include "Core/SlimeGameplayTags.h"

UGA_Fire::UGA_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Cannot shoot while dead or reloading.
	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Weapon_Reloading);
}
