// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "SlimeWarCoreTypes.generated.h"

class UStaticMesh;
class AActor;

/** Ecology allegiance. C is reserved for the later three-site layout. */
UENUM(BlueprintType)
enum class ESlimeFaction : uint8
{
	A UMETA(DisplayName = "Faction A"),
	B UMETA(DisplayName = "Faction B"),
	C UMETA(DisplayName = "Faction C")
};

/** Whether a body has an armed core, and whether that core is hostile. */
UENUM(BlueprintType)
enum class ESlimeCoreState : uint8
{
	None,
	Neutral,
	Hostile
};

/** Per-death scoring and statistics payload. */
USTRUCT(BlueprintType)
struct FSlimeKillContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	int32 Mass = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	int32 Cargo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	int32 BodyScoreCredit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	ESlimeFaction Faction = ESlimeFaction::A;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	ESlimeCoreState CoreState = ESlimeCoreState::None;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	bool bHostileCore = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	bool bKilledByPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	bool bDetonated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	bool bChainKill = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Score")
	FVector Location = FVector::ZeroVector;

	TWeakObjectPtr<AActor> Causer;
};

USTRUCT(BlueprintType)
struct FSlimeFusionEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Fusion")
	int32 ResultMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Fusion")
	int32 Cargo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Fusion")
	int32 BodyScoreCredit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Fusion")
	bool bHostileCore = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Fusion")
	FVector Location = FVector::ZeroVector;
};

/**
 * Deprecated: the legacy spawn-point HUD event still exposes this type so the existing
 * WBP_SlimeHUD compiles. Nothing in the runtime produces point states any more; M9 replaces
 * the HUD binding with ecology site data and removes this enum.
 */
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
	PlayerDied	UMETA(DisplayName = "Player Died"),
	Extinction	UMETA(DisplayName = "Ecology Extinction")
};

/**
 * High level run phase (Phase C, extended in Phase D).
 *
 * The five values are in lifecycle order and the run only ever moves forward through them:
 *
 *   Idle      -> the world is up and the run has not started. This IS the preparation phase
 *                (drop point selection); there is deliberately no separate "Preparing" value.
 *   Deploying -> the deployment cinematic is playing. Nothing has spawned, the countdown has not
 *                started and the player cannot act, because URunSubsystem::StartRun has not run.
 *   Running   -> batches are being driven and score can still be earned.
 *   Result    -> the run ended and the result camera is playing. Enemies and the player are frozen
 *                (design 5.6), the score is locked, and the settlement screen is not up yet.
 *   Ended     -> terminal. The settlement screen is up; only the retry / reselect / quit actions
 *                are still meaningful. Retrying reloads the level, so this state is never left
 *                in place.
 */
UENUM(BlueprintType)
enum class ESlimeRunState : uint8
{
	Idle		UMETA(DisplayName = "Idle (Preparation)"),
	Deploying	UMETA(DisplayName = "Deploying"),
	Running		UMETA(DisplayName = "Running"),
	Result		UMETA(DisplayName = "Result"),
	Ended		UMETA(DisplayName = "Ended")
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

	/**
	 * Collision body radius in cm.
	 *
	 * Careful: the design table quotes *diameters* (0.4 m at mass 1, 1.2 m at mass 8), so the
	 * CSV stores half of those values (20 ... 60). Keeping them as radii means the capsule never
	 * has to grow past the character's half height, which is what made the big slimes sink into
	 * the floor and get stuck.
	 */
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

/**
 * Drop-point layout. The class name and DA_SpawnLayout asset paths are kept so the Phase D
 * preparation/level-select assets keep loading, but the legacy spawn-point array is gone:
 * ecology sites and mineral patches now live as level actors.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeSpawnLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Player drop points (D1/D2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FVector> DropPoints;
};
