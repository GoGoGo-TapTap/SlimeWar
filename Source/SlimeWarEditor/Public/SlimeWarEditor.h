// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSlimeSpawnPointVisualizer;

/**
 * Editor tooling module.
 *
 * Registers, in order:
 *  - the spawn point / slot marker component visualizers (viewport drawing),
 *  - the detail customisations that carry the tool buttons,
 *  - the "PIE is about to start" consistency check.
 *
 * The upgrade path to a visualizer with real drag handles (option B) or a dedicated UEdMode
 * (option C) is additive: the runtime module owns nothing editor specific, so only this module
 * grows.
 */
class FSlimeWarEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Warns (never auto-writes) when the level markers differ from DA_SpawnLayout. */
	void OnPreBeginPIE(bool bIsSimulating);

	TSharedPtr<FSlimeSpawnPointVisualizer> SpawnPointVisualizer;
	FDelegateHandle PreBeginPIEHandle;
};
