// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Effects/GE_FireCooldown.h"
#include "Core/SlimeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_FireCooldown::UGE_FireCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_FireCooldown;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// GAS gates re-activation through UGameplayAbility::GetCooldownTags(), which reads
	// UGameplayEffect::GetGrantedTags() -- a cache that is only filled by
	// UTargetTagsGameplayEffectComponent. Writing the legacy InheritableOwnedTagsContainer
	// directly still compiles but grants nothing, so the tag must go through the component.
	// Structure only: the duration still comes from Data.FireCooldown.
	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(TAG_State_Weapon_Cooldown);

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);
}
