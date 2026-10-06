// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"

USlimeHealthComponent::USlimeHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeHealthComponent::InitializeHealth(float InMaxHealth, float HealthFraction)
{
	MaxHealth = FMath::Max(0.f, InMaxHealth);
	Health = MaxHealth * FMath::Clamp(HealthFraction, 0.f, 1.f);
	bDead = false;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void USlimeHealthComponent::ApplyDamage(float Amount, AActor* Causer)
{
	if (!bAuthoritative)
	{
		// The player AttributeSet is the single source of truth: never apply damage here.
		UE_LOG(LogSlimeWar, Warning,
			TEXT("[%s] ApplyDamage ignored on a non authoritative (proxy) health component."),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (Amount <= 0.f || bDead)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] took %.2f damage from %s -> %.2f / %.2f"),
			*GetNameSafe(GetOwner()), Amount, *GetNameSafe(Causer), Health, MaxHealth);
	}

	OnHealthChanged.Broadcast(Health, MaxHealth);
	BroadcastDeathIfNeeded();
}

void USlimeHealthComponent::RestoreToFull()
{
	Health = MaxHealth;
	bDead = false;
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void USlimeHealthComponent::SetHealthFromProxy(float NewHealth, float NewMaxHealth)
{
	MaxHealth = FMath::Max(0.f, NewMaxHealth);
	Health = FMath::Clamp(NewHealth, 0.f, MaxHealth);

	OnHealthChanged.Broadcast(Health, MaxHealth);
	BroadcastDeathIfNeeded();
}

void USlimeHealthComponent::BroadcastDeathIfNeeded()
{
	if (bDead || Health > 0.f || MaxHealth <= 0.f)
	{
		return;
	}

	bDead = true;

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] died."), *GetNameSafe(GetOwner()));
	}

	OnDeath.Broadcast();
}
