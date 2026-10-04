// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Effects/GE_Invulnerable.h"
#include "Core/SlimeGameplayTags.h"

UGE_Invulnerable::UGE_Invulnerable()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_HitProtectionDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}
