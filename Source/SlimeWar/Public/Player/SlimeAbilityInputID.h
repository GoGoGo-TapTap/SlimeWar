// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlimeAbilityInputID.generated.h"

/**
 * Ability input ids forwarded to UAbilitySystemComponent::AbilityLocalInputPressed/Released.
 * The actual UInputAction assets are created in Phase A (task PA-03); Phase 0 only routes them.
 */
UENUM(BlueprintType)
enum class EAbilityInputID : uint8
{
	None	UMETA(DisplayName = "None"),
	Fire	UMETA(DisplayName = "Fire"),
	Reload	UMETA(DisplayName = "Reload"),
	Aim		UMETA(DisplayName = "Aim"),
	Pause	UMETA(DisplayName = "Pause")
};
