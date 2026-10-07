// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The "normal targets only act inside their own point" rule (design 5.1 / 5.4), as pure math.
 *
 * Lives in Core so the enemy, the debug HUD and the automation tests all use one definition, and
 * so the rule can be covered without spawning an actor.
 */
namespace SlimeActivityArea
{
	/** A missing radius or an unset centre means "no constraint" (cheat spawns, hand-placed actors). */
	inline bool IsConstrained(const FVector& Center, float Radius)
	{
		return Radius > 0.f && !Center.IsNearlyZero();
	}

	inline bool IsInside(const FVector& Location, const FVector& Center, float Radius, float SlackCm = 0.f)
	{
		if (!IsConstrained(Center, Radius))
		{
			return true;
		}

		const float Limit = Radius + FMath::Max(0.f, SlackCm);
		return FVector::DistSquared2D(Location, Center) <= FMath::Square(Limit);
	}

	/** Pull a position back onto the activity circle. X/Y only; the Z is preserved. */
	inline FVector Clamp(const FVector& Location, const FVector& Center, float Radius)
	{
		if (!IsConstrained(Center, Radius))
		{
			return Location;
		}

		const FVector FlatOffset(Location.X - Center.X, Location.Y - Center.Y, 0.f);
		const float Distance = FlatOffset.Size();

		if (Distance <= Radius || Distance <= KINDA_SMALL_NUMBER)
		{
			return Location;
		}

		const FVector Direction = FlatOffset / Distance;
		return FVector(
			Center.X + Direction.X * Radius,
			Center.Y + Direction.Y * Radius,
			Location.Z);
	}
}
