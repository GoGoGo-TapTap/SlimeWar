// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Effects/GE_Invulnerable.h"
#include "Core/SlimeGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Invulnerable::UGE_Invulnerable()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_HitProtectionDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	// While this effect is active the target owns State.Player.Invulnerable.
	// USlimeCombatSubsystem reads that tag and drops incoming damage (design 4.6.3).
	// Granted tags must go through the component: GetGrantedTags() reads its cache.
	// Structure only: the duration still comes from Data.HitProtectionDuration.
	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(TAG_State_Player_Invulnerable);

	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);
}
