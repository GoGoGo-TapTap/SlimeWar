// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWarEditor.h"

#include "Core/SlimeWarLog.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "Flow/SpawnPoint.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SlimeSpawnEditorUtils.h"
#include "SlimeSpawnPointDetails.h"
#include "SlimeSpawnPointVisualizer.h"
#include "SlimeSpawnSlotMarkerDetails.h"
#include "UnrealEdGlobals.h"

void FSlimeWarEditorModule::StartupModule()
{
	// -- viewport drawing (registered for both the point icon and the slot handles) --
	if (GUnrealEd)
	{
		SpawnPointVisualizer = MakeShared<FSlimeSpawnPointVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(
			USlimeSpawnPointAnchor::StaticClass()->GetFName(), SpawnPointVisualizer);
		GUnrealEd->RegisterComponentVisualizer(
			USlimeSpawnSlotMarker::StaticClass()->GetFName(), SpawnPointVisualizer);
	}

	// -- detail panels (the tool buttons) --
	FPropertyEditorModule& PropertyEditor =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyEditor.RegisterCustomClassLayout(
		ASpawnPoint::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSlimeSpawnPointDetails::MakeInstance));

	PropertyEditor.RegisterCustomClassLayout(
		USlimeSpawnSlotMarker::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FSlimeSpawnSlotMarkerDetails::MakeInstance));

	PropertyEditor.NotifyCustomizationModuleChanged();

	// -- "the level and the asset disagree" warning before Play --
	PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FSlimeWarEditorModule::OnPreBeginPIE);
}

void FSlimeWarEditorModule::ShutdownModule()
{
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	PreBeginPIEHandle.Reset();

	if (GUnrealEd && SpawnPointVisualizer.IsValid())
	{
		GUnrealEd->UnregisterComponentVisualizer(USlimeSpawnPointAnchor::StaticClass()->GetFName());
		GUnrealEd->UnregisterComponentVisualizer(USlimeSpawnSlotMarker::StaticClass()->GetFName());
	}
	SpawnPointVisualizer.Reset();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyEditor.UnregisterCustomClassLayout(ASpawnPoint::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomClassLayout(USlimeSpawnSlotMarker::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
}

void FSlimeWarEditorModule::OnPreBeginPIE(bool bIsSimulating)
{
	// Deliberately a warning, never an automatic write: the level markers are the editing surface,
	// DA_SpawnLayout is the runtime truth, and only the designer decides when to bake one into the
	// other.
	const USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout();
	if (!Layout)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("Spawn point check: DA_SpawnLayout is not set in Project Settings -> Game -> Slime War; ")
			TEXT("the run will have no spawn points."));
		return;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	int32 OutOfSyncCount = 0;

	for (TActorIterator<ASpawnPoint> It(World); It; ++It)
	{
		const ASpawnPoint* Point = *It;
		if (!Point)
		{
			continue;
		}

		int32 MarkerCount = 0;
		int32 DefCount = 0;
		if (SlimeSpawnEditor::IsPointInSync(*Point, *Layout, MarkerCount, DefCount))
		{
			continue;
		}

		++OutOfSyncCount;
		UE_LOG(LogSlimeWar, Warning,
			TEXT("Spawn point %d is OUT OF SYNC with DA_SpawnLayout: level has %d slot handle(s), ")
			TEXT("the asset has %d. The run uses the ASSET - press 'Export To Data Asset' on the point ")
			TEXT("if the viewport layout is what you want."),
			Point->PointId, MarkerCount, DefCount);
	}

	if (OutOfSyncCount > 0)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("Spawn point check: %d point(s) are out of sync. See the warnings above."),
			OutOfSyncCount);
	}
}

IMPLEMENT_MODULE(FSlimeWarEditorModule, SlimeWarEditor)
