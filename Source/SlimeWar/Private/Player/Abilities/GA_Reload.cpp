// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Reload.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeWarLog.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"

UGA_Reload::UGA_Reload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationRequiredTags.AddTag(TAG_State_Player_Controllable);
	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Player_Result);

	// While reloading, firing is blocked but movement and aiming keep working.
	ActivationOwnedTags.AddTag(TAG_State_Weapon_Reloading);
}

void UGA_Reload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetAvatarActorFromActorInfo());
	USlimeWeaponComponent* WeaponComponent = Character ? Character->GetWeaponComponent() : nullptr;

	const float ReloadDuration = WeaponComponent ? WeaponComponent->GetReloadDuration() : 0.f;

	if (!WeaponComponent || WeaponComponent->IsMagazineFull() || ReloadDuration <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadDuration);
	if (!WaitTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitTask->OnFinish.AddDynamic(this, &UGA_Reload::HandleReloadFinished);
	WaitTask->ReadyForActivation();
}

void UGA_Reload::HandleReloadFinished()
{
	if (const ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetAvatarActorFromActorInfo()))
	{
		if (USlimeWeaponComponent* WeaponComponent = Character->GetWeaponComponent())
		{
			WeaponComponent->RefillMagazine();
		}
	}

	// Infinite reserve ammo (design 3.3); only the magazine is ever refilled.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
