// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlimePreparationWidget.generated.h"

/**
 * Preparation / drop point screen (PD-05 + PD-06).
 *
 * The overhead picture is a static image and the markers are placed by hand in the WBP; the only
 * thing C++ knows is the marker INDEX, because DA_SpawnLayout::DropPoints is an ordered array.
 * That deliberately avoids projecting world coordinates onto a UMG canvas.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimePreparationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/** Number of drop points to draw; the WBP binds one marker per index. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnPreparationReady(int32 DropPointCount);

	/**
	 * Hover / first click (design 2.3): remember the marker and let the WBP show the preview.
	 * The WBP calls this; confirming is a separate, explicit action.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void PreviewDropPoint(int32 DropPointIndex);

	/** Fired by PreviewDropPoint so the WBP can move / highlight its preview marker. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnDropPointPreviewed(int32 DropPointIndex);

	/** Second click, or the confirm button: this is the actual "choose a drop point" action. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void ConfirmDropPoint(int32 DropPointIndex);

	/** Same as ConfirmDropPoint, but keeps the currently previewed index (confirm button path). */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void ConfirmPreviewedDropPoint();

	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	int32 GetPreviewedDropPoint() const { return PreviewedDropPoint; }

protected:
	int32 PreviewedDropPoint = INDEX_NONE;
};
