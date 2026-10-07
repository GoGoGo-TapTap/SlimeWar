// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayFramework/SlimeGameSettings.h"

USlimeCombatSubsystem* USlimeCombatSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<USlimeCombatSubsystem>() : nullptr;
}

void USlimeCombatSubsystem::ApplyDamageTo(AActor* Target, float Amount, AActor* Causer)
{
	if (!Target || Amount <= 0.f)
	{
		return;
	}

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("ApplyDamageTo: %.2f damage to %s (causer %s)"),
			Amount, *GetNameSafe(Target), *GetNameSafe(Causer));
	}

	// Path A: the target owns an ASC (the player) -> GameplayEffect with a SetByCaller amount.
	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		// Hit protection (design 4.6.3): while State.Player.Invulnerable is up, incoming hits
		// neither deal damage nor refresh anything. The attacker still pays its recovery.
		if (TargetASC->HasMatchingGameplayTag(TAG_State_Player_Invulnerable))
		{
			if (SlimeCVars::DebugCombatLog != 0)
			{
				UE_LOG(LogSlimeWar, Log, TEXT("ApplyDamageTo: %s is invulnerable, %.2f damage dropped."),
					*GetNameSafe(Target), Amount);
			}
			return;
		}

		const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
		UClass* EffectClass = Settings ? Settings->DamageEffectClass.LoadSynchronous() : nullptr;

		if (!EffectClass)
		{
			UE_LOG(LogSlimeWar, Error,
				TEXT("ApplyDamageTo: DamageEffectClass is not set in Project Settings -> Game -> Slime War. ")
				TEXT("No damage was applied to %s."), *GetNameSafe(Target));
			return;
		}

		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(Causer, Causer);

		FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogSlimeWar, Error, TEXT("ApplyDamageTo: failed to build a spec for %s."), *EffectClass->GetName());
			return;
		}

		// GE_Damage adds its SetByCaller magnitude to Health, so hand it a negative value.
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Damage, -Amount);
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		return;
	}

	// Path B: no ASC (every enemy) -> the health component is authoritative.
	if (USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>())
	{
		Health->ApplyDamage(Amount, Causer);
		return;
	}

	// Neither path applies: this is scenery (floor, static meshes, props). Shooting the world is a
	// normal thing to do, so do not warn about it - only surface it when the combat log is on.
	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("ApplyDamageTo: %s has no damage path (no ASC, no USlimeHealthComponent), %.2f damage dropped."),
			*GetNameSafe(Target), Amount);
	}
}
