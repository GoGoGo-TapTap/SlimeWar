// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeSpawnSlotMarker.h"

namespace
{
	/** Editor handle colours. Presentation only, never gameplay values. */
	constexpr float MarkerArrowLength = 60.f;
	constexpr float MarkerArrowSize = 0.35f;
	constexpr float AnchorArrowLength = 140.f;
	constexpr float AnchorArrowSize = 0.9f;
}

USlimeSpawnSlotMarker::USlimeSpawnSlotMarker()
{
	bIsEditorOnly = true;

	ArrowLength = MarkerArrowLength;
	ArrowSize = MarkerArrowSize;
	ArrowColor = FColor(80, 220, 120); // Normal
}

USlimeSpawnPointAnchor::USlimeSpawnPointAnchor()
{
	bIsEditorOnly = true;

	ArrowLength = AnchorArrowLength;
	ArrowSize = AnchorArrowSize;
	ArrowColor = FColor(80, 200, 255);
}
