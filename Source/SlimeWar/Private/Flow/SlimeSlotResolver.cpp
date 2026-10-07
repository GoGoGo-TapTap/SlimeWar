// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeSlotResolver.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

namespace
{
	/** Debug/validation geometry. These are probes, not balance values. */
	constexpr float ProbeSphereRadius = 42.f;
	constexpr float ProbeSphereHeight = 100.f;
	constexpr float GroundTraceUp = 300.f;
	constexpr float GroundTraceDown = 1000.f;
	/** Navmesh projection extent around the desired point. */
	constexpr float NavProjectionExtent = 200.f;
	/** Keeps the capsule from starting exactly flush with the floor. */
	constexpr float SpawnZClearance = 2.f;
}

namespace SlimeSlotResolver
{
	FResolution Resolve(
		const UWorld& World,
		const AActor* IgnoreActor,
		const FVector& DesiredWorld,
		float PlayerMinDistance)
	{
		FResolution Result;

		// 1) Must sit on the navmesh: the StateTree AI cannot use a slot off the navmesh.
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
		if (NavSystem)
		{
			FNavLocation Projected;
			if (!NavSystem->ProjectPointToNavigation(
				DesiredWorld,
				Projected,
				FVector(NavProjectionExtent, NavProjectionExtent, NavProjectionExtent)))
			{
				Result.Reason = ESlotRejectReason::NoNavMesh;
				return Result;
			}
		}

		// 2) Must be standable: find the ground under the desired XY. The desired Z is only a hint,
		//    which is what lets a marker be dragged into the air (or sit slightly under the floor).
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeSpawnSlot), /*bTraceComplex=*/false);
		if (IgnoreActor)
		{
			QueryParams.AddIgnoredActor(IgnoreActor);
		}

		const FVector GroundStart = DesiredWorld + FVector(0.f, 0.f, GroundTraceUp);
		const FVector GroundEnd = DesiredWorld - FVector(0.f, 0.f, GroundTraceDown);

		FHitResult GroundHit;
		if (!World.LineTraceSingleByChannel(GroundHit, GroundStart, GroundEnd, ECC_WorldStatic, QueryParams))
		{
			Result.Reason = ESlotRejectReason::NoGround;
			return Result;
		}

		Result.GroundZ = GroundHit.ImpactPoint.Z;
		Result.Location = FVector(DesiredWorld.X, DesiredWorld.Y, Result.GroundZ);

		// 3) Room for the body: a coarse sphere above the ground still catches walls and props,
		//    but can no longer be dragged into the floor itself.
		if (World.OverlapBlockingTestByChannel(
			Result.Location + FVector(0.f, 0.f, ProbeSphereHeight),
			FQuat::Identity,
			ECC_WorldStatic,
			FCollisionShape::MakeSphere(ProbeSphereRadius),
			QueryParams))
		{
			Result.Reason = ESlotRejectReason::Blocked;
			return Result;
		}

		// 4) Far enough from the player (design 5.2).
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(&World, 0);
		if (PlayerPawn)
		{
			Result.PlayerDistance = FVector::Dist2D(PlayerPawn->GetActorLocation(), Result.Location);

			if (PlayerMinDistance > 0.f && Result.PlayerDistance < PlayerMinDistance)
			{
				Result.Reason = ESlotRejectReason::TooCloseToPlayer;
				return Result;
			}
		}

		Result.bValid = true;
		return Result;
	}

	float GetSpawnZ(float GroundZ, float CapsuleHalfHeight)
	{
		return GroundZ + CapsuleHalfHeight + SpawnZClearance;
	}

	const TCHAR* RejectReasonName(ESlotRejectReason Reason)
	{
		switch (Reason)
		{
		case ESlotRejectReason::NoNavMesh:			return TEXT("off the navmesh");
		case ESlotRejectReason::NoGround:			return TEXT("no ground under the slot");
		case ESlotRejectReason::Blocked:			return TEXT("blocked by static geometry");
		case ESlotRejectReason::TooCloseToPlayer:	return TEXT("too close to the player");
		default:									return TEXT("no usable candidate");
		}
	}
}
