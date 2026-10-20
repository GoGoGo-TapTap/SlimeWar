// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SlimeAimPreviewWidget.generated.h"

class ASlimeWarCharacter;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;

/**
 * Aim overlay: the charge ring, the charge percentage and the two crosshairs.
 *
 * Visuals stay in UMG (WBP_AimPreview is already authored); this base class only drives them, so
 * the layout artist keeps full control and the logic has exactly one implementation.
 *
 * Shown only while the run is Running: USlimeUISubsystem toggles the widget's visibility per phase,
 * which is why the widget itself never decides "am I allowed to exist".
 *
 * Unlike the HUD (which is push-based), this reads two pure getters every frame on purpose: the
 * ring is continuous 0..1 data, not a discrete event.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeAimPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Aim")
	TObjectPtr<UImage> Img_ChargeRing_Fill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Aim")
	TObjectPtr<UTextBlock> Text_ChargePercent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Aim")
	TObjectPtr<UImage> Img_Crosshair_Default;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Aim")
	TObjectPtr<UImage> Img_Crosshair_Weakpoint;

	/** Scalar parameter on MI_ChargeRing, 0..1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Aim")
	FName ChargePercentParameter = TEXT("ChargePercent");

private:
	/** Pull the aim state from the player and push it into the widgets. */
	void RefreshAimState();

	ASlimeWarCharacter* ResolveCharacter();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChargeRingMID;

	TWeakObjectPtr<ASlimeWarCharacter> CachedCharacter;

	/** Last values, so the widget only touches the tree when something actually changed. */
	float LastChargeAlpha = -1.f;
	int32 LastChargePercent = -1;
	bool bLastWeakpoint = false;
	bool bLastCharging = false;
};
