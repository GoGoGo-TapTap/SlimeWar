// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeLevelDefinition.generated.h"

class USlimeRunConfig;
class UUserWidget;
class UWorld;

/**
 * One playable level: its map, its numbers, its drop point screen and its deployment cinematics.
 *
 * This is the only asset that knows "which level is which". Everything that used to be global and
 * should have been per level is reached through it, so adding a level is one asset plus one ini
 * line - no C++ change.
 *
 * It lives in GameplayFramework because it references USlimeRunConfig. The widget class is kept as
 * a plain UUserWidget soft class (not USlimePreparationWidget) so this header never has to include
 * a UI header, which is the same rule USlimeGameSettings follows.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeLevelDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Stable id used by the session and by saves. Never use the array index instead: the index
	 * drifts as soon as a level is inserted, and stored indices then point at the wrong level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FName LevelId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText DisplayName;

	/** The map to open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	TSoftObjectPtr<UWorld> MapAsset;

	/**
	 * Numbers for this level (duration, target score, batches, player tuning) and, through its
	 * SpawnLayout, this level's drop point count and coordinates.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	TSoftObjectPtr<USlimeRunConfig> RunConfig;

	/**
	 * Drop point screen for this level.
	 * Empty falls back to USlimeGameSettings::PreparationWidgetClass, which keeps the single-level
	 * setup working while the per-level WBPs are being authored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|UI")
	TSoftClassPtr<UUserWidget> PreparationWidgetClass;

	/**
	 * One deployment cinematic per drop point; index = USlimeSpawnLayout::DropPoints index.
	 * Empty falls back to USlimeGameSettings::DeploySequences.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|UI")
	TArray<FSoftObjectPath> DeploySequences;

	/** Settlement orbit. Empty falls back to USlimeGameSettings::ResultSequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|UI")
	FSoftObjectPath ResultSequence;
};
