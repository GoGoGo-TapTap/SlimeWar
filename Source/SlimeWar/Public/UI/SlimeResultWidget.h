// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Flow/RunSubsystem.h"
#include "SlimeResultWidget.generated.h"

class UTexture;
class UButton;

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
	virtual void NativeDestruct() override;

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

	/**
	 * The settlement photo taken by the level's ASlimeResultCamera.
	 * Null when the level has no camera or nothing was captured yet - the WBP then falls back to
	 * T_Result_BG. The render target is not a UTexture2D, so show it through the M_ResultPhoto
	 * material instead of SetBrushFromTexture.
	 */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	UTexture* GetResultPhoto() const;

	/** Whether the next configured level exists and has a map. Drives the "next level" button. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	bool HasNextLevel() const;

	/** Enter the next level's preparation screen. False (and a log line) when there is none. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	bool RequestNextLevel();

	/** Void Blueprint entry point used by the result page to avoid a bool return through a world transition. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestNextLevelDeferred();

	/** Open the level select screen: screen switch only, the world is not reloaded. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestLevelSelect();

	/**
	 * Fired once, when the screen is actually shown (phase == Ended).
	 *
	 * Numbers stay on OnResultReady. Animation and sound MUST use this one: OnResultReady also runs
	 * when the level starts, with an all-zero result, so anything hooked there would play long
	 * before the settlement screen appears.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnResultShown();

protected:
	UFUNCTION()
	void HandleNextLevelClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_NextLevel;
};
