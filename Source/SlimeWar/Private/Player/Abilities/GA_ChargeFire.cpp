// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_ChargeFire.h"

#include "AbilitySystemComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeWarLog.h"
#include "Player/Abilities/GA_Reload.h"
#include "Player/Effects/GE_FireCooldown.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"
#include "Player/SlimeWeaponRules.h"

UGA_ChargeFire::UGA_ChargeFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationRequiredTags.AddTag(TAG_State_Player_Controllable);
	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Weapon_Reloading);
	ActivationBlockedTags.AddTag(TAG_State_Player_Result);
	CooldownGameplayEffectClass = UGE_FireCooldown::StaticClass();
}

void UGA_ChargeFire::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const USlimeWeaponRules* Rules = USlimeWeaponRules::GetFor(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!ASC || !CooldownGameplayEffectClass || !Rules)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo), Context);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		TAG_Data_FireCooldown,
		FMath::Max(0.01f, Rules->FireInterval));
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UGA_ChargeFire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetAvatarActorFromActorInfo());
	USlimeWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	const USlimeWeaponRules* Rules = USlimeWeaponRules::GetFor(Character);
	if (!Character || !Weapon || !Rules)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Weapon->IsMagazineEmpty())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->TryActivateAbilityByClass(UGA_Reload::StaticClass());
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	AActor* HitActor = nullptr;
	bool bWeakpoint = false;
	Character->PerformChargedShot(
		Character->ConsumeChargeAlpha(),
		ImpactPoint,
		HitActor,
		bWeakpoint);
	Weapon->ConsumeShot();
	const bool bNeedsReload = Weapon->IsMagazineEmpty();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

	if (bNeedsReload && ASC)
	{
		ASC->TryActivateAbilityByClass(UGA_Reload::StaticClass());
	}
}
