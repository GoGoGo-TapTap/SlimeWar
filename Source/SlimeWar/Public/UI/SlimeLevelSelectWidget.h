// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeLevelSelectWidget.generated.h"

class USlimeLevelDefinition;

/**
 * Level picker.
 *
 * Four slots are reserved, matching the four nodes drawn in WBP_SlimeLevelSelect. Which slot points
 * at which map is data (USlimeGameSettings::Levels, one asset per level), not code: a slot whose
 * entry is missing can still be selected - so the widget can show it as locked - but cannot be
 * entered. NumLevels is the UI slot count, not the number of configured levels.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeLevelSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** UI slot count. Keep the nodes in WBP_SlimeLevelSelect in sync; the configured level count is separate. */
	static constexpr int32 NumLevels = 4;

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void SelectLevel(int32 LevelIndex);

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void EnterSelectedLevel();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	int32 GetSelectedLevel() const { return SelectedLevelIndex; }

	/** Total number of slots, including the reserved ones. Handy if the picker is built from data. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	int32 GetLevelCount() const { return NumLevels; }

	/** Number of levels actually configured. Always <= GetLevelCount(). */
	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	int32 GetConfiguredLevelCount() const;

	/** Stable id of the level in this slot; empty when the slot is not configured. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	FName GetLevelId(int32 LevelIndex) const;

	/** Display name of the level in this slot; empty when the slot is not configured. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	FText GetLevelDisplayName(int32 LevelIndex) const;

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	bool IsLevelAvailable(int32 LevelIndex) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI|Level Select")
	void OnLevelSelectionChanged(int32 LevelIndex, bool bAvailable);

protected:
	/** Range check only: a valid slot may still be locked. */
	bool IsValidLevelIndex(int32 LevelIndex) const { return LevelIndex >= 0 && LevelIndex < NumLevels; }

	/** Slot -> level definition; null when the slot is out of range or not configured. */
	const USlimeLevelDefinition* ResolveLevel(int32 LevelIndex) const;

	int32 SelectedLevelIndex = 0;
};
