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
 * Which of the three slot lists an editing marker belongs to.
 *
 * Only the editor markers carry this: inside FSlimeSpawnPointDef a slot's role is implied by the
 * array it lives in (design 5.2 uses separate positions for normal and aggressive slimes).
 */
UENUM(BlueprintType)
enum class ESlimeSlotRole : uint8
{
	Normal		UMETA(DisplayName = "Normal"),
	Aggro		UMETA(DisplayName = "Aggressive"),
	Fallback	UMETA(DisplayName = "Fallback")
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
 * Aggressive slime stats. Aggressive individuals do not fuse and have no mass tier,
 * so they get their own table row instead of a row inside DT_SlimeStats.
 *
 * Keyed in DT_AggroStats by row name (Phase A uses "Default").
 * PLACEHOLDER: every numeric column is pending design confirmation (plan section 8 Q6,
 * plus Q13 for the attack damage which the design document never states).
 * Numbers must live in the DataTable, never in code.
 */
USTRUCT(BlueprintType)
struct FSlimeAggroStatRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Aggressive individuals do not grow, so this is a flat value (design: 60). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float MaxHealth = 0.f;

	/** Chase speed in cm/s (design: 4.8 m/s for the design 6 m/s player). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float MoveSpeed = 0.f;

	/** Collision body radius in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float BodyRadius = 0.f;

	/** Stop-and-attack distance in cm (design: 1.2 m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float AttackRange = 0.f;

	/** Damage of one landed attack. PLACEHOLDER: the design document does not state it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float AttackDamage = 0.f;

	/** Wind-up before the hit check, seconds (design: 0.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float AttackWindupTime = 0.f;

	/** Recovery after the hit check, seconds (design: 0.35). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float AttackRecoverTime = 0.f;

	/** Minimum time between two attack starts, seconds (design: 1.6). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	float AttackCooldown = 0.f;

	/** Static mesh for this archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aggro")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

/**
 * One spawnable slot inside a spawn point, expressed in the anchor's local space
 * (8 normal slots + 2 aggro slots per point).
 *
 * Everything is relative to the ASpawnPoint actor, so moving or rotating the anchor moves the
 * whole layout without touching this asset.
 */
USTRUCT(BlueprintType)
struct FSlimeSpawnSlot
{
	GENERATED_BODY()

	/**
	 * Anchor-local offset in cm.
	 * Only X and Y are authoritative: the Z is re-resolved to the ground under that XY at runtime,
	 * so a slot dragged into the air still produces a slime standing on the floor below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FVector RelativeLocation = FVector::ZeroVector;

	/** Anchor-local facing, degrees. The slime turns to face its movement right after spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

/**
 * One spawn point definition (Phase C, PC-01).
 *
 * The centre is NOT stored here: ASpawnPoint's own transform is the centre, and every slot below
 * is an offset in that anchor's local space. The layout lives in DA_SpawnLayout so the runtime
 * never reads the level's editor markers (plan 7.3 + the Phase C tooling decision).
 *
 * Slot counts are not enforced by the struct: the number of slots per batch and the number of
 * batches come from USlimeRunConfig, so the design values stay in data.
 */
USTRUCT(BlueprintType)
struct FSlimeSpawnPointDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 PointId = 0;

	/**
	 * Activity radius around the anchor, cm. 0 = fall back to USlimeRunConfig::AIActivityRadius.
	 * Kept in data (not on the anchor) so it stays tunable without touching the binary map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float ActivityRadius = 0.f;

	/** Primary normal spawn offsets, one per normal slot in a batch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FSlimeSpawnSlot> NormalSlots;

	/** Primary aggressive spawn offsets, one per aggro slot in a batch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FSlimeSpawnSlot> AggroSlots;

	/** Backup offsets tried when a primary slot fails the standable / distance check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FSlimeSpawnSlot> FallbackSlots;
};

/**
 * Data driven spawn layout (DA_SpawnLayout). Keeps the binary map free of spawn data
 * so the map stays cheap to edit and merge.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeSpawnLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** One entry per spawn point. Phase C sandbox only fills a single point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FSlimeSpawnPointDef> Points;

	/** Player drop points (D1/D2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FVector> DropPoints;
};
