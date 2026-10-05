// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimePlayerAttributeSet.h"
#include "Core/SlimeWarLog.h"
#include "GameplayEffectExtension.h"
#include "Player/SlimeWarCharacter.h"

USlimePlayerAttributeSet::USlimePlayerAttributeSet()
{
	// All attributes intentionally stay at 0 until data is applied (rule 5).
}

void USlimePlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void USlimePlayerAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

		UE_LOG(LogSlimeWar, Verbose, TEXT("Player health is now %.2f / %.2f"), GetHealth(), GetMaxHealth());

		// PA-07: direction hook for the Phase D damage indicator. The causer is carried in the
		// effect context by USlimeCombatSubsystem, so this is the only place that knows it.
		if (Data.EvaluatedData.Magnitude < 0.f)
		{
			if (ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetOwningActor()))
			{
				Character->NotifyDamagedFrom(Data.EffectSpec.GetContext().GetInstigator());
			}
		}
	}
}
