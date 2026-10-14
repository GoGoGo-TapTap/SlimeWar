// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeLevelSelectWidget.generated.h"

/**
 * First version of the level picker.
 *
 * Index 0 is CityPlaza. Indices 1 and 2 are reserved for future content and deliberately remain
 * unavailable until their maps and progression rules exist.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeLevelSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void SelectLevel(int32 LevelIndex);

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void EnterSelectedLevel();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Level Select")
	void SetReturnWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	int32 GetSelectedLevel() const { return SelectedLevelIndex; }

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Level Select")
	bool IsLevelAvailable(int32 LevelIndex) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI|Level Select")
	void OnLevelSelectionChanged(int32 LevelIndex, bool bAvailable);

protected:
	TWeakObjectPtr<UUserWidget> ReturnWidget;
	int32 SelectedLevelIndex = 0;
};
