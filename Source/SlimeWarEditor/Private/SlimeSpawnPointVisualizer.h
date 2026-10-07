// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"

class ASpawnPoint;

/**
 * Draws a spawn point in the viewport: the activity circle, the anchor-to-slot links and the slot
 * positions, plus a HUD label with the point id and its sync state.
 *
 * Registered for both USlimeSpawnPointAnchor and USlimeSpawnSlotMarker, so it appears whether the
 * designer selects the point icon or one of its slot handles.
 *
 * This is the hook the future drag-handle upgrade (option B) grows into: adding
 * HandleClick/HandleDrag to this class is local to the editor module.
 */
class FSlimeSpawnPointVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(
		const UActorComponent* Component,
		const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;

	virtual void DrawVisualizationHUD(
		const UActorComponent* Component,
		const FViewport* Viewport,
		const FSceneView* View,
		FCanvas* Canvas) override;

private:
	static const ASpawnPoint* GetOwningPoint(const UActorComponent* Component);

	static void DrawCircle(
		FPrimitiveDrawInterface* PDI,
		const FVector& Center,
		float Radius,
		const FLinearColor& Color);
};
