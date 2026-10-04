// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Reload.h"
#include "Core/SlimeGameplayTags.h"

UGA_Reload::UGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);

	// While reloading, firing is blocked but movement and aiming keep working.
	ActivationOwnedTags.AddTag(TAG_State_Weapon_Reloading);
}
