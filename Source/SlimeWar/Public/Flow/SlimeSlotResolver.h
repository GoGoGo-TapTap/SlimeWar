// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/** Why a spawn slot was rejected. Logged on cancellation so a bad layout diagnoses itself. */
enum class ESlotRejectReason : uint8
{
	None,
	/** The point is not on the navmesh, so the StateTree AI could not use it. */
	NoNavMesh,
	/** No ground under the slot inside the trace range. */
	NoGround,
	/** Something static occupies the body space above the ground. */
	Blocked,
	/** Closer to the player than SpawnPlayerMinDistance (design 5.2: at least 3 m). */
	TooCloseToPlayer
};

/**
 * Turns a desired world position into a usable spawn position.
 *
 * This is the single implementation shared by the runtime spawner and the editor's
 * "Validate Layout" / "Snap Slots To Ground" tools, so what the tool reports is exactly what the
 * game will do (plan: Editor and Runtime stay separated, but they must not drift apart).
 *
 * The desired Z is only a hint: a slime has to stand on something, so the ground under the
 * desired XY wins (design 5.2). That is what makes a slot dragged into the air still work and it
 * is what removes the "probe buried in the floor" false positive.
 */
namespace SlimeSlotResolver
{
	struct FResolution
	{
		/** True when Location is usable. */
		bool bValid = false;

		/** Resolved ground point: spawn X/Y plus the ground Z. Invalid when !bValid. */
		FVector Location = FVector::ZeroVector;

		float GroundZ = 0.f;

		/** 2D distance to the player, or -1 when there is no player pawn. */
		float PlayerDistance = -1.f;

		ESlotRejectReason Reason = ESlotRejectReason::None;
	};

	/**
	 * @param DesiredWorld      where the slot wants the slime, world space
	 * @param PlayerMinDistance cm; <= 0 disables the player distance check
	 */
	SLIMEWAR_API FResolution Resolve(
		const UWorld& World,
		const AActor* IgnoreActor,
		const FVector& DesiredWorld,
		float PlayerMinDistance);

	/** Actor origin Z for a character whose feet must rest on GroundZ. */
	SLIMEWAR_API float GetSpawnZ(float GroundZ, float CapsuleHalfHeight);

	SLIMEWAR_API const TCHAR* RejectReasonName(ESlotRejectReason Reason);
}
