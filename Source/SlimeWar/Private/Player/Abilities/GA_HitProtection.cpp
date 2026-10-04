// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_HitProtection.h"
#include "Core/SlimeGameplayTags.h"

UGA_HitProtection::UGA_HitProtection()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
}
