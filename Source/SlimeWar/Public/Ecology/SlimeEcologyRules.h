// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeEcologyRules.generated.h"

/**
 * M2 ecology tuning. The defaults make a newly-created asset immediately usable; the canonical
 * runtime copy lives in DA_EcologyRules so values can be changed without recompiling.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeEcologyRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Resolve the level/provider-owned rules, falling back to the class defaults. */
	static const USlimeEcologyRules* GetFor(const UObject* WorldContextObject);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
	float PerceptionInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
	float AllyPerceptionRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
	float SightHeightTolerance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Group")
	float GroupExitDistance = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Group")
	float GroupEnterDistance = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance")
	float PlayerAvoidEnterDistance = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance")
	float PlayerAvoidExitDistance = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance")
	float PlayerAvoidSpeed = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance")
	float AvoidCandidateDistance = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance")
	int32 AvoidCandidateCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float ExploreSpeed = 115.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float RegroupSpeed = 115.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float ExtraSpacing = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SeparationWeight = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SeparationOverlayWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float DirectionSmoothing = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float MoveAcceleration = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float BrakingDeceleration = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float TurnRateDegPerSecond = 360.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float WanderRadiusMin = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float WanderRadiusMax = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float WanderMinimumDisplacement = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float WanderArrivalRadius = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float WanderSelectionInterval = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	int32 WanderCandidateCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	float PathRefreshInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	float WaypointAcceptanceRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	float StuckCheckSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	float StuckDistance = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation")
	int32 MaxPathFailures = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spatial Hash")
	float SpatialCellSize = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	int32 BodyPushMaxMass = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float BodyPushBaseSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float BodyPushPlayerSpeedScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float BodyPushMassExponent = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float BodyPushContactPadding = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float ExternalVelocityDamping = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float StunStopSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float StunSettleTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float HitStunDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float SplitStunDuration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float ForcedFusionWindow = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float ImpactSplitSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Interaction")
	float ImpactSplitWindow = 0.9f;
};
