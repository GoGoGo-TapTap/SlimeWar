// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_Fire.h"

#include "AbilitySystemComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeWarLog.h"
#include "Player/Abilities/GA_Reload.h"
#include "Player/Effects/GE_FireCooldown.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"

UGA_Fire::UGA_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Firing needs control of the character: blocked while deploying, dead or in the result screen.
	ActivationRequiredTags.AddTag(TAG_State_Player_Controllable);
	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
	ActivationBlockedTags.AddTag(TAG_State_Weapon_Reloading);
	ActivationBlockedTags.AddTag(TAG_State_Player_Result);

	// Native cooldown: the effect grants State.Weapon.Cooldown, which is what GAS checks.
	CooldownGameplayEffectClass = UGE_FireCooldown::StaticClass();
}

void UGA_Fire::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !CooldownGameplayEffectClass)
	{
		return;
	}

	const ASlimeWarCharacter* Character = ActorInfo ? Cast<ASlimeWarCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USlimeWeaponComponent* WeaponComponent = Character ? Character->GetWeaponComponent() : nullptr;
	const float FireRate = WeaponComponent ? WeaponComponent->GetFireRate() : 0.f;

	if (FireRate <= 0.f)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GA_Fire::ApplyCooldown: fire rate is not configured, no cooldown applied."));
		return;
	}

	// Rule 5: the effect holds structure only; the interval is injected from the weapon row.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(CooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo), Context);

	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GA_Fire::ApplyCooldown: failed to build the cooldown spec."));
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_FireCooldown, 1.f / FireRate);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UGA_Fire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetAvatarActorFromActorInfo());
	USlimeWeaponComponent* WeaponComponent = Character ? Character->GetWeaponComponent() : nullptr;

	if (!Character || !WeaponComponent || WeaponComponent->GetFireRate() <= 0.f || WeaponComponent->GetRange() <= 0.f)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("GA_Fire::ActivateAbility: the player has no usable weapon (check DA_RunConfig / DT_WeaponStats)."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Design 3.3: an empty magazine starts a reload instead of a shot.
	if (WeaponComponent->IsMagazineEmpty())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->TryActivateAbilityByClass(UGA_Reload::StaticClass());
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Commit first: even a shot that ends up hitting a wall costs the full fire interval.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector ImpactPoint = FVector::ZeroVector;
	AActor* HitActor = nullptr;
	Character->PerformShot(ImpactPoint, HitActor);

	WeaponComponent->ConsumeShot();

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
