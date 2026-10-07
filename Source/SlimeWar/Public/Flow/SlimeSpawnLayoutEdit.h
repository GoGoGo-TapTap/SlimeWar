// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"

/**
 * Pure helpers behind the spawn point editor's DA round trip.
 *
 * They live in the runtime module (not in the editor module) so the sorting / duplicate rules can
 * be unit tested without an editor, and so both sides agree on what "the layout" means.
 */
namespace SlimeSpawnLayoutEdit
{
	/** One marker as read from the level, before it is folded into a FSlimeSpawnPointDef. */
	struct FSlotEntry
	{
		ESlimeSlotRole Role = ESlimeSlotRole::Normal;
		int32 Index = 0;
		FVector RelativeLocation = FVector::ZeroVector;
		FRotator RelativeRotation = FRotator::ZeroRotator;
	};

	/**
	 * Fold markers into the three arrays.
	 *
	 * Entries are sorted by Index inside each role, so the array order is exactly the slot order
	 * the spawner walks. Duplicate indices are kept in their sorted order (the tool warns about
	 * them), and roles with no entries produce empty arrays (the runtime then reuses what it has).
	 */
	SLIMEWAR_API void BuildSlotArrays(
		const TArray<FSlotEntry>& Entries,
		TArray<FSlimeSpawnSlot>& OutNormalSlots,
		TArray<FSlimeSpawnSlot>& OutAggroSlots,
		TArray<FSlimeSpawnSlot>& OutFallbackSlots);

	/** Read the three arrays back into one flat marker list (used by "Import From Data Asset"). */
	SLIMEWAR_API void FlattenSlotArrays(
		const TArray<FSlimeSpawnSlot>& NormalSlots,
		const TArray<FSlimeSpawnSlot>& AggroSlots,
		const TArray<FSlimeSpawnSlot>& FallbackSlots,
		TArray<FSlotEntry>& OutEntries);

	/**
	 * True when two slots of the same array sit at (nearly) the same XY.
	 *
	 * Hand-authored assets used to leave unused elements at (0,0,0); a local offset of zero is a
	 * legitimate position now, so instead of guessing "unset" the spawner reports this once and
	 * points at the editor tool.
	 */
	SLIMEWAR_API bool HasDuplicateXY(const TArray<FSlimeSpawnSlot>& Slots, float ToleranceCm = 1.f);
}
