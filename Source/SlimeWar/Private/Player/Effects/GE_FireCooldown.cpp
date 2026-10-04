// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Effects/GE_FireCooldown.h"
#include "Core/SlimeGameplayTags.h"

UGE_FireCooldown::UGE_FireCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_FireCooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
