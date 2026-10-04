// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Effects/GE_Damage.h"
#include "Core/SlimeGameplayTags.h"
#include "Player/SlimePlayerAttributeSet.h"

UGE_Damage::UGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USlimePlayerAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Modifier);
}
