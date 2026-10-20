// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Engine/DataAsset.h"
#include "SlimeEcologyRules.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FSlimeFactionVisual
{
	GENERATED_BODY()

	/** Body mesh used by this faction. Empty keeps the mass-table/default body mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	/** Material applied to the body and faction-colored attachments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialInterface> BodyMaterial;

	/** Deferred-decal material for death splats, one per faction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;
};

/**
 * M2 ecology tuning. The defaults make a newly-created asset immediately usable; the canonical
 * runtime copy lives in DA_EcologyRules so values can be changed without recompiling.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeEcologyRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USlimeEcologyRules();

	/** Resolve the level/provider-owned rules, falling back to the class defaults. */
	static const USlimeEcologyRules* GetFor(const UObject* WorldContextObject);

	/** Faction body/material record; null when the faction has no explicit override. */
	const FSlimeFactionVisual* FindFactionVisual(ESlimeFaction Faction) const;

	/** Faction color from the data asset, falling back to the v2 palette. */
	FLinearColor GetFactionColorOrDefault(ESlimeFaction Faction) const;

	// -- Faction / attachment visuals --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TMap<ESlimeFaction, FSlimeFactionVisual> FactionVisuals;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TMap<ESlimeFaction, FLinearColor> FactionColors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> SpikeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> CoreMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> WeakpointMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialInterface> CoreMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UMaterialInterface> WeakpointMaterial;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionAutoPrepMin = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionAutoPrepMax = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionAutoSeekRange = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionAutoSeekSpeed = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionContactTime = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionContactTolerance = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionForcedCaptureGap = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionSuccessCooldown = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionInterruptedBlock = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	int32 FusionAutoMassCap = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	int32 FusionTotalMassCap = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float FusionSeekTimeout = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fusion")
	float ShotFusionCooldown = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreWakeTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreSenseRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreChaseSpeed = 380.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreCombatMemory = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreLeashDistance = 1900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CorePlayerProximity = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CorePlayerPriorityProximity = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CorePlayerProvokedDuration = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreRetreatHealthFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreRetreatDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreAttackInterval = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreWindup = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core")
	float CoreRecovery = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float PounceBaseRange = 340.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float PounceSpeed = 1050.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float PounceDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float PounceLift = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float PounceHitHeight = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float CoreDamagePlayer = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pounce")
	float CoreDamageRival = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Devour")
	float DevourHealPerMass = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionBaseRadius = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionRadiusPerMass = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionDamage = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionEdgeScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionPush = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	float ExplosionPushMassExponent = 0.45f;

	/** Neutral cores damage neighbours but only hostile cores recurse into another detonation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explosion")
	bool HostileCoresChainOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weakpoint")
	int32 WeakpointStartMass = 7;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weakpoint")
	float WeakpointRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weakpoint")
	float WeakpointHeight = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float ResourceSenseRange = 800.f;

	/** Long-range fallback used after returning to a site, where nearby patches are empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float ResourceExplorationRange = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float ResourceMemory = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float ResourceTargetHold = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float GatherRange = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float GatherTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float DeliveryRange = 130.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float PickupRange = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float PickupActionRange = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float CargoSpeed = 115.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float ExploreSpeedWithResourceMemory = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float InitialGatherRestMin = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float InitialGatherRestMax = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float MineralCrowdingRadius = 140.f;

	/** Cost in metres added for each visible faction member near a mineral patch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float MineralCrowdingPenalty = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float GatherRestMin = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource")
	float GatherRestMax = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ecology Spawn")
	float SiteSpawnRingMin = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ecology Spawn")
	float SiteSpawnRingMax = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ecology Spawn")
	float SiteSpawnPlayerMinDistance = 270.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ecology Spawn")
	int32 SpawnRequestsPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float AlertWarningTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float AlertDuration = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float AlertPropagationRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float AlertDangerSpeed = 154.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float CompetitorSenseRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float HostileCoreAvoidCarrying = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float HostileCoreAvoidEmpty = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alert")
	float AlertPostEmptyGatherRest = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dispute")
	float DisputeRange = 340.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dispute")
	float DisputeSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dispute")
	float DisputeCooldown = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	int32 GrenadesPerType = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float GrenadeThrowInterval = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float GrenadeThrowSpeed = 1300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float GrenadeThrowLift = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float GrenadeGravity = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float GrenadeFlightTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float AssimilationRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoDuration = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoAcceleration = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoMaxSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoStopRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grenade")
	float TornadoFusionReadyTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 PaintTextureWidth = 700;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 PaintTextureHeight = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	int32 MaxPaintSplatRecords = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	float PaintSenseRange = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	float PaintApproachSpeed = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	float PaintStopRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float ExplorationHistoryCellSize = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Explore")
	float ExplorationVisitPenalty = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FVector2D PaintWorldSize = FVector2D(3000.f, 4400.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TSoftObjectPtr<UMaterialInterface> PaintSplatMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TSoftObjectPtr<UMaterialInterface> PaintSurfaceMaterial;

	/** Deferred-decal material used for one decal actor per splat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TSoftObjectPtr<UMaterialInterface> PaintDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FName PaintTextureParameter = TEXT("SlimePaint");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FName PaintSplatUVParameter = TEXT("SplatUV");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FName PaintSplatColorParameter = TEXT("SplatColor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FName PaintSplatRadiusParameter = TEXT("SplatRadius");
};
