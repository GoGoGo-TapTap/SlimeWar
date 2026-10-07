// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Flow/RunSubsystem.h"
#include "SlimeResultWidget.generated.h"

/**
 * Settlement screen (PD-10).
 *
 * Shown only once the phase reaches Ended - i.e. after the result orbit finished - so the buttons
 * never sit on top of the cinematic (PD-11).
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** All settlement numbers in one struct (see FSlimeRunResult). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnResultReady(const FSlimeRunResult& Result);

	/**
	 * Re-read the settlement data and fire OnResultReady.
	 *
	 * The widgets are all created once when the level starts, so NativeConstruct runs long before
	 * the run ends; the UI subsystem calls this again the moment the screen is actually shown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RefreshResult();

	/** Retry the same drop point (reloads the level, keeps the best score). */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestRetry();

	/** Back to the preparation screen. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestReselectDropPoint();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestQuit();

	/** Convenience for the WBP: the same rule the settlement screen displays. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	bool WasPassed() const;
};
