// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "SlimeSettingsWidget.generated.h"

/**
 * Video and audio settings screen.
 *
 * Display values are applied through UGameUserSettings. The master volume value is staged and
 * persisted for the future Wwise integration; audio output is intentionally untouched until the
 * Wwise RTPC/Game Parameter is available.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void SetResolution(int32 Width, int32 Height);

	/** 0 = windowed, 1 = fullscreen, 2 = windowed fullscreen. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void SetFullscreenMode(int32 Mode);

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void ResetSettings();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void ReturnToPreviousMenu();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void SetReturnWidget(UUserWidget* Widget);

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Settings")
	float GetMasterVolume() const { return PendingMasterVolume; }

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Settings")
	FIntPoint GetResolution() const { return PendingResolution; }

	UFUNCTION(BlueprintPure, Category = "Slime|UI|Settings")
	int32 GetFullscreenMode() const;

	/**
	 * Returns resolutions reported by the platform. The current resolution is included even when
	 * the platform does not expose a list, which keeps the combo box usable in editor/PIE.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI|Settings")
	void GetSupportedResolutions(TArray<FIntPoint>& OutResolutions) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI|Settings")
	void OnSettingsValuesChanged();

protected:
	/** Reserved for Wwise 2024.1.8.8898 RTPC integration. Currently a deliberate no-op. */
	void ApplyMasterVolumeToAudio();

	TWeakObjectPtr<UUserWidget> ReturnWidget;
	float PendingMasterVolume = 1.0f;
	FIntPoint PendingResolution = FIntPoint::ZeroValue;
	TEnumAsByte<EWindowMode::Type> PendingFullscreenMode = EWindowMode::Windowed;
};
