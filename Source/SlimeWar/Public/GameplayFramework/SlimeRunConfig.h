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

	/** Invulnerability window after being hit, in seconds (design: 0.6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	float HitProtectionDuration = 0.f;

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
