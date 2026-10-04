// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "SlimeAbilitySystemGlobals.generated.h"

/**
 * Project GAS globals. Phase 0 keeps it empty: it exists as the hook for future project wide
 * GAS defaults (cue manager, custom effect context, ...).
 *
 * UE 5.5 calls InitGlobalData() automatically the first time the globals are requested
 * (FGameplayAbilitiesModule::GetAbilitySystemGlobals), so there is nothing to do in code.
 *
 * Registration lives in Config/DefaultGame.ini:
 *   [/Script/GameplayAbilities.GameplayAbilitiesDeveloperSettings]
 *   AbilitySystemGlobalsClassName=/Script/SlimeWar.SlimeAbilitySystemGlobals
 */
UCLASS()
class USlimeAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
};
