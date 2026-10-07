// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

/**
 * Per-handle panel: the "Remove Slot" button and the role-driven recolour.
 *
 * This lives in the editor module on purpose - the runtime component stays pure data.
 */
class FSlimeSpawnSlotMarkerDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
