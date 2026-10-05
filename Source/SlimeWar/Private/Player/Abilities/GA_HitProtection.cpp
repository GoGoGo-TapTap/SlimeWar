// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/Abilities/GA_HitProtection.h"

#include "AbilitySystemComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeWarLog.h"
#include "Engine/GameInstance.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Player/Effects/GE_Invulnerable.h"

UGA_HitProtection::UGA_HitProtection()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ActivationBlockedTags.AddTag(TAG_State_Player_Dead);
	// Never stack two protection windows.
	ActivationBlockedTags.AddTag(TAG_State_Player_Invulnerable);
}

void UGA_HitProtection::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStatTableProvider* Provider = GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;
	const float Duration = RunConfig ? RunConfig->HitProtectionDuration : 0.f;

	if (Duration <= 0.f)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("GA_HitProtection: HitProtectionDuration is 0 in DA_RunConfig, no protection applied."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Structure only: the window length is injected through SetByCaller (rule 5).
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Avatar, Avatar);

	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(UGE_Invulnerable::StaticClass(), GetAbilityLevel(Handle, ActorInfo), Context);

	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_HitProtectionDuration, Duration);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
