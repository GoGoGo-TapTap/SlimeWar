// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeCombatSubsystem.generated.h"

class AActor;

/**
 * The single damage entry point for the whole project.
 *
 * It hides the two damage paths behind one call:
 *   - target owns an AbilitySystemComponent (player) -> apply the configured GameplayEffect,
 *     the amount is injected with SetByCaller "Data.Damage";
 *   - target has no ASC (every enemy)               -> forward to USlimeHealthComponent.
 *
 * Callers (weapon, enemy attack, cheats) never branch on GAS themselves.
 * No GAS type may appear in this header.
 */
UCLASS()
class USlimeCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null if the world context is not usable. */
	UFUNCTION(BlueprintPure, Category = "Slime|Combat", meta = (WorldContext = "WorldContextObject"))
	static USlimeCombatSubsystem* Get(const UObject* WorldContextObject);

	/** Apply Amount damage to Target. Does nothing for a null target or a non positive amount. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Combat")
	void ApplyDamageTo(AActor* Target, float Amount, AActor* Causer);
};
