// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SlimeGameSettings.generated.h"

class UGameplayEffect;
class USlimeLevelDefinition;
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

	// -- Levels --

	/**
	 * The level list. Array order IS the level select slot order, which is why this is a TArray and
	 * not a TMap: "+Levels=" lines in the ini stay readable in the order they are written.
	 *
	 * An empty list is supported (the project still runs from a single map opened in PIE).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Levels")
	TArray<TSoftObjectPtr<USlimeLevelDefinition>> Levels;

	/** Level definition for a level select slot; null when the slot is not configured. */
	const USlimeLevelDefinition* FindLevelByIndex(int32 Index) const;

	/** Level definition by stable id; null when nothing matches (including an empty id). */
	const USlimeLevelDefinition* FindLevelById(FName LevelId) const;

	/**
	 * Position of a level in Levels, or INDEX_NONE when the id is empty or unknown.
	 *
	 * This is what "next level" is built on: index + 1 in the configured order. Deliberately not a
	 * name comparison, so reordering the list moves the progression with it.
	 */
	int32 IndexOfLevel(FName LevelId) const;

	/** Resolve a level when a map is opened directly in PIE or by a console Open command. */
	const USlimeLevelDefinition* FindLevelByMapName(const FString& MapName) const;

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

	/**
	 * Aim overlay (charge ring + crosshairs). Created and shown only while the run is Running.
	 * Leaving it empty keeps the game playable without a crosshair, so it degrades like the others.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> AimPreviewWidgetClass;

	/**
	 * Fallback drop point screen, used when the current level does not configure its own
	 * (see USlimeLevelDefinition::PreparationWidgetClass).
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> PreparationWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> ResultWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> PauseWidgetClass;

	/** Main menu flow screens. These are independent from the in-run UI subsystem. */
	UPROPERTY(config, EditAnywhere, Category = "UI|Menus")
	TSoftClassPtr<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI|Menus")
	TSoftClassPtr<UUserWidget> SettingsWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "UI|Menus")
	TSoftClassPtr<UUserWidget> LevelSelectWidgetClass;

	/**
	 * Fallback deployment cinematics, one per drop point (index = DA_SpawnLayout drop point index).
	 * Levels normally configure their own in USlimeLevelDefinition::DeploySequences; this list is
	 * only read when the level leaves it empty.
	 *
	 * FSoftObjectPath rather than TSoftObjectPtr so the value serialises cleanly in an ini list.
	 * A missing entry means "no cinematic": the director confirms the deployment immediately.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TArray<FSoftObjectPath> DeploySequences;

	/** Fallback settlement orbit camera, used when the level does not configure one. */
	UPROPERTY(config, EditAnywhere, Category = "UI")
	FSoftObjectPath ResultSequence;

	/**
	 * Editor convenience: start PIE directly on the preparation screen of the current map instead of
	 * stopping at the main menu first.
	 *
	 * **Off by default**: starting a session must look the same in PIE and in a packaged build, or
	 * "it works in editor" stops meaning anything. Turn it on only while debugging a single map's
	 * combat or drop point screen, and remember that with it on you cannot test the menu flow.
	 *
	 * Ignored when the session explicitly asked for the main menu
	 * (USlimeSessionSubsystem::RequestMainMenu), so leaving a run always lands on the main menu.
	 * Has no effect outside the editor.
	 */
	UPROPERTY(config, EditAnywhere, Category = "UI|Editor")
	bool bSkipMainMenuInPIE = false;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
