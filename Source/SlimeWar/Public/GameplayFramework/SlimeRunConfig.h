// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeRunConfig.generated.h"

class UDataTable;
class USlimeSpawnLayout;

/**
 * Single source of truth for run numbers and data table references (DA_RunConfig).
 *
 * Every value defaults to 0 on purpose: numbers only ever live in this asset, never in code
 * (plan rule 5). Fill them in the editor and they are changeable without recompiling.
 */
UCLASS(BlueprintType)
class USlimeRunConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// -- Run flow --

	/** In-run countdown in seconds (design: 180). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	float RunDuration = 0.f;

	/** Score needed to pass the run (design: 300). Reaching it does NOT end the run early. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	int32 TargetScore = 0;

	/** Deployment cinematic length in seconds (design: 2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	float DeployDuration = 0.f;

	/** Result orbit camera length in seconds (design: 2~3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	float ResultOrbitDuration = 0.f;

	// -- Player --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerMaxHealth = 0.f;

	/** cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerMoveSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerTurnRateDegPerSec = 0.f;

	/** Ground acceleration in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerAcceleration = 0.f;

	/** Braking deceleration while walking in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerBrakingDeceleration = 0.f;

	/** Invulnerability window after being hit, in seconds (design: 0.6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float HitProtectionDuration = 0.f;

	// -- Camera / weapon (TPS shoulder view) --

	/** Spring arm length in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraBoomLength = 0.f;

	/** Spring arm socket offset, cm. Positive Y pushes the camera to the character's right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector CameraSocketOffset = FVector::ZeroVector;

	/** Camera field of view, degrees. 0 keeps the engine default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraFieldOfView = 0.f;

	/** Muzzle offset from the camera, in camera space (cm). X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector MuzzleOffsetLocal = FVector::ZeroVector;

	/** Aim assist cone half angle in degrees, measured from the camera forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float AimAssistMaxAngleDeg = 0.f;

	// -- Normal slime AI (Phase A: StateTree, numbers stay in data) --

	/** Delay after spawning before a normal slime starts looking for a partner, seconds (design: 2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AISpawnWaitTime = 0.f;

	/** Activity radius around the spawn point, cm (design: 6 m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AIActivityRadius = 0.f;

	/** Max wander step when no partner is found, cm (design: <= 3 m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AIWanderRadius = 0.f;

	/** Wander pause lower bound, seconds (design: 0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AIWanderPauseMin = 0.f;

	/** Wander pause upper bound, seconds (design: 1.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AIWanderPauseMax = 0.f;

	/** Give-up time for a single approach / wander step, seconds (design: 2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	float AIApproachTimeout = 0.f;

	/** Mass sum cap for fusion (design: 8). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Normal")
	int32 AIFusionMassCap = 0;

	// -- Fusion (Phase B) --

	/** Continuous contact needed before two slimes fuse, seconds (design 4.4: 0.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Fusion")
	float FusionContactTime = 0.f;

	/**
	 * Extra slack added to the two capsule radii when testing "touching", cm.
	 * Small on purpose: it only absorbs the frame step, it is not a magnet range.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Fusion")
	float FusionContactTolerance = 0.f;

	/** Wait after a successful fusion before the fused body may look again, seconds (design 4.4: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Fusion")
	float FusionPostFusionDelay = 0.f;

	/** Wait after a cancelled attempt before looking again, seconds (design 4.6.2: 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Fusion")
	float FusionRetryDelay = 0.f;

	/**
	 * Participants per fusion. Phase B always uses 2; the resolve code folds over the list, so
	 * raising this is the only data change a future multi-way fusion needs (see the component).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Fusion")
	int32 FusionMaxParticipants = 2;

	// -- Aggressive slime --

	/** DT_AggroStats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Aggro")
	TSoftObjectPtr<UDataTable> AggroStatTable;

	/** Row name used inside the aggro table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Aggro")
	FName AggroRowName;

	// -- Data tables --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> SlimeStatTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> WeaponStatTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<USlimeSpawnLayout> SpawnLayout;

	/** Row name used in DT_WeaponStats for the starting weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName DefaultWeaponId;
};
