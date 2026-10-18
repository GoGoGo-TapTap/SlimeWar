// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimePauseWidget.generated.h"

/**
 * Pause menu (PD-13).
 *
 * The key is the one bound to IA_Pause (Escape in the design configuration).
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimePauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void Resume();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestRetry();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestReselectDropPoint();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestQuit();
};
