// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimeMainMenuWidget.generated.h"

class UUserWidget;

/**
 * Entry screen for the game.
 *
 * The WBP owns layout, artwork and button visuals. The UI subsystem owns the overall menu flow.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Main Menu")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Main Menu")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Main Menu")
	void QuitGame();

protected:
	void OpenMenuWidget(TSubclassOf<UUserWidget> WidgetClass);
};
