// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "SlimePlayerAttributeSet.generated.h"

/**
 * UE 5.5 does NOT define ATTRIBUTE_ACCESSORS: AttributeSet.h only documents the pattern in a
 * comment and ships the four GAMEPLAYATTRIBUTE_* macros. Define the convenience macro here.
 */
#define SLIME_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Player attributes. The file name must match the class name for UHT
 * (SlimePlayerAttributeSet.h, not PlayerAttributeSet.h as the plan text said).
 *
 * Every value starts at 0 on purpose: numbers come from DA_RunConfig / DT_WeaponStats
 * (rule 5). UI never reads this class directly, it reads USlimeHealthComponent instead,
 * which is why this header can stay in Private/.
 */
UCLASS()
class USlimePlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USlimePlayerAttributeSet();

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData WeaponDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData MagazineSize;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData ReloadDuration;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Attributes")
	FGameplayAttributeData FireRate;

	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, Health);
	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, MaxHealth);
	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, WeaponDamage);
	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, MagazineSize);
	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, ReloadDuration);
	SLIME_ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, FireRate);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
