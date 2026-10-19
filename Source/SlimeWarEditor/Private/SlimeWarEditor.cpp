// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWarEditor.h"

#include "AI/NavigationSystemBase.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

// The Phase C spawn-point editor tooling was retired with the legacy batch design. The module is
// kept alive because SlimeWar.uproject and the editor target still list it; M9 can either grow
// ecology authoring tools here or remove the module entirely.

namespace
{
	void BakeCurrentMapNavigation()
	{
		if (!GEditor)
		{
			return;
		}

		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (!World)
		{
			return;
		}

		FNavigationSystem::Build(*World);

		// The build is asynchronous: wait for the editor tick to process the navmesh tiles, then
		// save the level so a later -game / packaged session can use the baked data.
		static FTimerHandle SaveTimer;
		World->GetTimerManager().SetTimer(
			SaveTimer,
			FTimerDelegate::CreateLambda([]()
			{
				UWorld* EditorWorld = GEditor
					? GEditor->GetEditorWorldContext().World()
					: nullptr;
				UNavigationSystemV1* Nav = EditorWorld
					? FNavigationSystem::GetCurrent<UNavigationSystemV1>(EditorWorld)
					: nullptr;
				UE_LOG(LogTemp, Warning, TEXT("SlimeBakeNav: build finished, nav system=%s"),
					Nav ? TEXT("yes") : TEXT("no"));
				FEditorFileUtils::SaveCurrentLevel();
				UE_LOG(LogTemp, Warning, TEXT("SlimeBakeNav: level saved."));
			}),
			8.f,
			false);
	}

	FAutoConsoleCommand SlimeBakeNavCommand(
		TEXT("SlimeBakeNav"),
		TEXT("Build navigation for the current editor map and save it."),
		FConsoleCommandDelegate::CreateStatic(&BakeCurrentMapNavigation));
}

IMPLEMENT_MODULE(FSlimeWarEditorModule, SlimeWarEditor)
