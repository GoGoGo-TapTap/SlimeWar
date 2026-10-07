// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"

class ASpawnPoint;
class USlimeRunConfig;
class USlimeSpawnLayout;
class USlimeSpawnSlotMarker;
class UWorld;

/**
 * The spawn point editor's operations, shared by the detail panel buttons and the PIE pre-check.
 *
 * Kept free of Slate so the tool logic is testable and so a future UEdMode (option C) can call the
 * same functions instead of reimplementing them.
 */
namespace SlimeSpawnEditor
{
	/** DA_SpawnLayout, resolved through Project Settings -> Slime War -> Run Config. */
	USlimeSpawnLayout* LoadSpawnLayout();
	USlimeRunConfig* LoadRunConfig();

	float GetPlayerMinDistance(const USlimeRunConfig* RunConfig);

	void CollectMarkers(const ASpawnPoint& Point, TArray<USlimeSpawnSlotMarker*>& OutMarkers);

	const FSlimeSpawnPointDef* FindPointDef(const USlimeSpawnLayout& Layout, int32 PointId);
	FSlimeSpawnPointDef* FindMutablePointDef(USlimeSpawnLayout& Layout, int32 PointId);

	/** Add one handle of the given role, with the next free index, at an anchor-local offset. */
	USlimeSpawnSlotMarker* AddMarker(ASpawnPoint& Point, ESlimeSlotRole Role, const FVector& RelativeLocation);

	/** Remove a handle (and unregister it from the actor). */
	void RemoveMarker(ASpawnPoint& Point, USlimeSpawnSlotMarker& Marker);

	/** Remove every handle of this anchor. */
	int32 RemoveAllMarkers(ASpawnPoint& Point);

	/** 8 + 2 + 3 handles on rings, using the batch sizes from the run config. */
	int32 CreateDefaultSlots(ASpawnPoint& Point, const USlimeRunConfig& RunConfig);

	/** Move every handle's Z onto the ground under its XY. Returns how many were snapped. */
	int32 SnapMarkersToGround(ASpawnPoint& Point, float PlayerMinDistance);

	/** Log one line per handle with the resolver verdict. Returns the number of unusable slots. */
	int32 ValidatePoint(const ASpawnPoint& Point, float PlayerMinDistance);

	/**
	 * Compare the level handles with the asset definition.
	 * @param OutMarkerCount  handles in the level
	 * @param OutDefCount     slots stored in the asset
	 */
	bool IsPointInSync(
		const ASpawnPoint& Point,
		const USlimeSpawnLayout& Layout,
		int32& OutMarkerCount,
		int32& OutDefCount);

	/** Write this anchor's handles into the layout (update-or-insert by PointId). */
	void ExportPoint(ASpawnPoint& Point, USlimeSpawnLayout& Layout, FString& OutSummary);

	/** Export every ASpawnPoint in the level. Never deletes definitions of absent points. */
	void ExportAllPoints(UWorld& World, USlimeSpawnLayout& Layout, FString& OutSummary);

	/** Rebuild this anchor's handles from the layout. */
	void ImportPoint(ASpawnPoint& Point, const USlimeSpawnLayout& Layout, FString& OutSummary);

	FLinearColor RoleColor(ESlimeSlotRole Role);
	void ApplyRoleVisuals(USlimeSpawnSlotMarker& Marker);
}
