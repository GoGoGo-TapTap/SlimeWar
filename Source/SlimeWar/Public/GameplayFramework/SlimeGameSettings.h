// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SlimeGameSettings.generated.h"

class UGameplayEffect;
class USlimeRunConfig;
class UUserWidget;

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

	// -- Phase D UI (PD-05 ~ PD-13) --

	/**
	 * Widget classes, as soft references so this header never has to include a UI header and the
	 * paths can be repointed from DefaultGame.ini without touching a binary asset.
	 *
	 * Leaving one empty is supported: the UI subsystem logs a warning and skips that screen, which
	 * is what makes the code side testable before the WBP assets exist.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> PreparationWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> ResultWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> PauseWidgetClass;

	/**
	 * Deployment cinematics, one per drop point (index = drop point index in DA_SpawnLayout).
	 *
	 * FSoftObjectPath rather than TSoftObjectPtr so the value serialises cleanly in an ini list.
	 * A missing entry means "no cinematic": the director confirms the deployment immediately.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TArray<FSoftObjectPath> DeploySequences;

	/** Settlement orbit camera. Empty means "no orbit": the run goes straight to the result screen. */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	FSoftObjectPath ResultSequence;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
