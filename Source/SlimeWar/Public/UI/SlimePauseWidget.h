// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimePauseWidget.generated.h"

/**
 * Pause menu (PD-13).
 *
 * The key is the one already bound to IA_Pause (P); the design document says Esc, and that
 * difference is registered in Docs/PhaseD/PhaseD-Plan.md §9.
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
