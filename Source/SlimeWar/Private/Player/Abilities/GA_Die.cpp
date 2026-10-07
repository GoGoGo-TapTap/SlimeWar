// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Die.h"

#include "AbilitySystemComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeWarLog.h"
#include "Player/SlimeWarCharacter.h"

UGA_Die::UGA_Die()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Dead is applied as a *loose* tag (it has to survive this ability ending), so here it is a
	// blocked tag instead of an owned one: the ability can never start twice.
	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
}

void UGA_Die::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetAvatarActorFromActorInfo());

	if (!ASC || !Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Cancel first, then tag: an in-flight reload is stopped before State.Player.Dead goes up,
	// so nothing can slip through the blocking tags in the same frame (design 3.1 / PA-10).
	// "this" is ignored so the death ability finishes on its own terms.
	ASC->CancelAllAbilities(this);

	// Tag, movement lock and the IBattleDirector report; bCancelAbilities=false because the
	// cancel above already ran (and must not cancel this ability while it is still executing).
	Character->ApplyDeathEffects(/*bCancelAbilities=*/false);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
