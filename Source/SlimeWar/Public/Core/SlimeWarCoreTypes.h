// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "SlimeWarCoreTypes.generated.h"

class UStaticMesh;

/**
 * Target kind. Normal slimes can fuse with each other and award score.
 * Aggressive slimes chase and attack the player and never award score.
 */
UENUM(BlueprintType)
enum class ETargetKind : uint8
{
	Normal		UMETA(DisplayName = "Normal Slime"),
	Aggressive	UMETA(DisplayName = "Aggressive Slime")
};

/** Spawn point lifecycle. Cleared only considers normal targets, never live aggro. */
UENUM(BlueprintType)
enum class ESpawnPointState : uint8
{
	AwaitingDeploy		UMETA(DisplayName = "Awaiting Deploy"),
	Spawning			UMETA(DisplayName = "Spawning"),
	DepletedNotCleared	UMETA(DisplayName = "Depleted Not Cleared"),
	Cleared				UMETA(DisplayName = "Cleared")
};

/** Run end reason. Only these two end a run. */
UENUM(BlueprintType)
enum class ERunEndReason : uint8
{
	None		UMETA(DisplayName = "None"),
	TimeUp		UMETA(DisplayName = "Time Up"),
	PlayerDied	UMETA(DisplayName = "Player Died")
};

/**
 * Per-mass slime stats, keyed in DT_SlimeStats by the mass value as row name ("1".."8").
 * PLACEHOLDER: every numeric column is pending design confirmation (plan section 8, Q1/Q7).
 * Numbers must live in the DataTable, never in code.
 */
USTRUCT(BlueprintType)
struct FSlimeStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Mass / body size tier, 1..8. Row name must match this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	int32 Mass = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	float MaxHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	float MoveSpeed = 0.f;

	/** Collision body radius in cm (design quotes 0.4m for mass 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	float BodyRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	int32 KillScore = 0;

	/** Static mesh for this mass tier (slimes are static meshes, not skeletal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

/**
 * Weapon stats, keyed in DT_WeaponStats by weapon id ("Rifle").
 * PLACEHOLDER: pending design confirmation (plan section 8, Q4/Q5).
 */
USTRUCT(BlueprintType)
struct FWeaponStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Damage per hit, injected into GE_Damage through SetByCaller "Data.Damage". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage = 0.f;

	/** Rounds per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineSize = 0;

	/** Reload duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ReloadDuration = 0.f;

	/** Max hit distance in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Range = 0.f;

	/** Aim assist strength, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float AimAssistStrength = 0.f;
};

/** One spawnable slot inside a spawn point (8 normal slots + 2 aggro slots per point). */
USTRUCT(BlueprintType)
struct FSlimeSpawnSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 PointId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	ETargetKind Kind = ETargetKind::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * Data driven spawn layout (DA_SpawnLayout). Keeps the binary map free of spawn data
 * so the map stays cheap to edit and merge.
 */
UCLASS(BlueprintType)
class USlimeSpawnLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FSlimeSpawnSlot> SpawnSlots;

	/** Player drop points (D1/D2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FVector> DropPoints;
};
