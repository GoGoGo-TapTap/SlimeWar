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

	/**
	 * Leave the preparation screen and go back to the main menu.
	 *
	 * This screen has no other way out: the UI subsystem's accessor is a plain C++ static (not a
	 * UFUNCTION), so a WBP cannot reach it. The forwarder lives here, next to the actions this
	 * screen already owns.
	 *
	 * Uses ReturnToMainMenu rather than ShowMainMenu on purpose: leaving the preparation screen
	 * means abandoning the attempt, so the world is rebuilt and the next launch starts clean.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestBackToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	int32 GetPreviewedDropPoint() const { return PreviewedDropPoint; }

	/**
	 * Stable id of the level this screen belongs to. Empty when PIE opened a map directly.
	 * A screen shared by several levels can branch on it; the normal case is one WBP per level,
	 * in which case the WBP does not need it.
	 */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	FName GetLevelId() const { return CachedLevelId; }

	/**
	 * Drop point count for this level - the same value OnPreparationReady received.
	 *
	 * Use this when the WBP needs the count again later (e.g. validating a marker index); do not
	 * read the data asset directly.
	 */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	int32 GetDropPointCount() const { return CachedDropPointCount; }

protected:
	/** Range check against the cached count. Shared so confirm cannot skip what preview enforces. */
	bool IsValidDropPoint(int32 DropPointIndex) const
	{
		return DropPointIndex >= 0 && DropPointIndex < CachedDropPointCount;
	}

	int32 PreviewedDropPoint = INDEX_NONE;

	/** Resolved once in NativeConstruct so hover-time range checks do not re-resolve the asset. */
	FName CachedLevelId;
	int32 CachedDropPointCount = 0;
};
