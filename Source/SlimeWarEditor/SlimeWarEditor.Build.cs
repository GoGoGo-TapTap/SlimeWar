// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * Editor-only tooling for the spawn point layout.
 *
 * Everything in here is presentation and asset authoring: the runtime module owns the data and the
 * spawn logic, this module owns the handles, the buttons and the DA round trip. Being an "Editor"
 * type module it is never compiled into a packaged game.
 */
public class SlimeWarEditor : ModuleRules
{
	public SlimeWarEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SlimeWar"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",				// GUnrealEd, component visualizer registry
			"ComponentVisualizers",	// FComponentVisualizer
			"PropertyEditor",		// IDetailCustomization
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorFramework",
			"ToolMenus",
			"DeveloperSettings",
			"GameplayTags",
			"RenderCore"			// FPrimitiveDrawInterface / FCanvasItem back end
		});
	}
}
