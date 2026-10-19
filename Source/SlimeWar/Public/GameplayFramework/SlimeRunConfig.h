// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeRunConfig.generated.h"

class UDataTable;
class USlimeSpawnLayout;
class USlimeEcologyRules;
class USlimeWeaponRules;

/**
 * Single source of truth for run numbers and data table references (DA_RunConfig).
 *
 * Every value defaults to 0 on purpose: numbers only ever live in this asset, never in code
 * (plan rule 5). Fill them in the editor and they are changeable without recompiling.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeRunConfig : public UPrimaryDataAsset
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

	/**
	 * Phase C: start the run automatically when the world begins play (sandbox behaviour).
	 * Phase D turns this off and calls URunSubsystem::StartRun after the deployment cinematic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run")
	bool bAutoStartRun = true;

	// -- Tutorial (Phase D, PD-12) --

	/**
	 * Radius in cm for the "first fusion nearby" hint (design 7.5).
	 * 0 means "no range limit": the hint then fires on the first fusion anywhere in the run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	float TutorialFusionProximity = 0.f;

	// -- Player --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerMaxHealth = 0.f;

	/** cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerMoveSpeed = 0.f;

	/** New R1 field: sprint speed in cm/s. Does not replace PlayerMoveSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerSprintSpeed = 0.f;

	/** New R1 field: aim speed in cm/s. Aiming takes priority over sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerAimSpeed = 0.f;

	/** New R1 field: jump launch speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerJumpSpeed = 0.f;

	/** New R1 field: gravity acceleration in cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float PlayerGravity = 0.f;

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

	/** New R1 field: spring arm length while aiming, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraAimBoomLength = 0.f;

	/** Spring arm socket offset, cm. Positive Y pushes the camera to the character's right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector CameraSocketOffset = FVector::ZeroVector;

	/** Camera field of view, degrees. 0 keeps the engine default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraFieldOfView = 0.f;

	/** New R1 field: camera FOV while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraAimFieldOfView = 0.f;

	/** Muzzle offset from the camera, in camera space (cm). X forward, Y right, Z up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector MuzzleOffsetLocal = FVector::ZeroVector;

	/** Aim assist cone half angle in degrees, measured from the camera forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float AimAssistMaxAngleDeg = 0.f;

	// -- Data tables --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> SlimeStatTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> WeaponStatTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<USlimeEcologyRules> EcologyRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<USlimeWeaponRules> WeaponRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<USlimeSpawnLayout> SpawnLayout;

	/** Row name used in DT_WeaponStats for the starting weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName DefaultWeaponId;

};
