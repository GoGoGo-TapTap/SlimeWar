// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SlimeGameSettings.generated.h"

class UGameplayEffect;
class USlimeRunConfig;

/**
 * Project settings for the Slime War module (Project Settings -> Game -> Slime War).
 *
 * A UGameInstanceSubsystem cannot expose its own asset references (there is no place to
 * assign them), so the data entry points live here and are stored in Config/DefaultGame.ini.
 * Text only: no binary asset needs to be touched to repoint a table or an effect.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Slime War"))
class SLIMEWAR_API USlimeGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Run numbers plus the two stat tables. Set to /Game/_SlimeWar/Core/Data/DA_RunConfig. */
	UPROPERTY(config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<USlimeRunConfig> RunConfig;

	/**
	 * GameplayEffect applied to any target that owns an AbilitySystemComponent (the player).
	 * Structure only: the amount is injected at apply time through SetByCaller "Data.Damage".
	 */
	UPROPERTY(config, EditAnywhere, Category = "Combat")
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
