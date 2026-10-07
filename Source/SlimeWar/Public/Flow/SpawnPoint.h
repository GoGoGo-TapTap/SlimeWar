// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Flow/SlimeSlotResolver.h"
#include "GameFramework/Actor.h"
#include "SpawnPoint.generated.h"

class ASlimeEnemyBase;
class USlimeRunConfig;
class USlimeSpawnPointAnchor;

/**
 * One spawn point (PC-01 ~ PC-05).
 *
 * The anchor IS the point: this actor's transform is the centre of the activity radius and the
 * origin every slot offset is relative to, so moving or rotating it moves the whole layout without
 * touching DA_SpawnLayout.
 *
 * The actor holds no layout at runtime - it reads its FSlimeSpawnPointDef from the asset, exactly
 * like every other number in this project. The draggable per-slot handles in the level are
 * USlimeSpawnSlotMarker components: editor-only, never read by the game.
 *
 * URunSubsystem owns the timeline and calls ExecuteBatch; this class owns everything inside one
 * batch: slot resolution (shared with the editor tool through SlimeSlotResolver), the 2 s delayed
 * retry and the four state names.
 *
 * Counting rule: "cleared" only ever looks at normal slimes. Live aggro slimes neither block the
 * clear nor disappear when their point is cleared (design 5.3 / 5.4).
 */
UCLASS()
class SLIMEWAR_API ASpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ASpawnPoint();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Identity. Must match a FSlimeSpawnPointDef::PointId in DA_SpawnLayout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 PointId = 0;

	/**
	 * Classes to spawn. Editable so a Blueprint subclass can be dropped in without touching the
	 * data asset; the defaults are the pure C++ classes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<ASlimeEnemyBase> NormalClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<ASlimeEnemyBase> AggroClass;

	/** Editor-only icon marking the point centre. It follows the actor's transform. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<USlimeSpawnPointAnchor> AnchorIcon;

	/** Called by URunSubsystem::StartRun before the first batch. */
	void InitializeRuntime(const FSlimeSpawnPointDef& InDefinition, const USlimeRunConfig& InRunConfig);

	/** Issue one batch (design: 8 normal + 2 aggro). Out of range batches are ignored. */
	void ExecuteBatch(int32 BatchIndex);

	/** Called when the run ends: drop the retry queue so no delayed spawn can still fire. */
	void StopSpawning();

	/** World position of this point's centre (the anchor transform). */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	FVector GetSpawnCentre() const { return GetActorLocation(); }

	/** DA value; 0 means "use USlimeRunConfig::AIActivityRadius". */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	float GetActivityRadius() const { return ActivityRadius; }

	/** Effective radius after the run config fallback, for the AI and the editor visualisation. */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	float GetEffectiveActivityRadius() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	ESpawnPointState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetPointId() const { return PointId; }

	/** Successful normal spawns so far (a cancelled spawn never counts). */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetSpawnedNormalCount() const { return SpawnedNormalCount; }

	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetSpawnedAggroCount() const { return SpawnedAggroCount; }

	/** Live normal slimes of this point. Aggro is deliberately excluded. */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetLiveNormalCount() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetLiveAggroCount() const;

	/** Total normal supply over the whole run (design: 6 * 8 = 48). */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetTotalNormalSupply() const { return TotalNormalSupply; }

	/** Spawns waiting out their retry window. */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	int32 GetPendingSpawnCount() const { return PendingSpawns.Num(); }

	/** Whether every batch has been issued and no delayed spawn is still pending. */
	UFUNCTION(BlueprintPure, Category = "Slime|Spawn")
	bool IsSupplyDone() const { return bSupplyDone; }

private:
	/** One delayed spawn: every candidate failed, we keep retrying until the deadline. */
	struct FPendingSpawn
	{
		ETargetKind Kind = ETargetKind::Normal;
		FSlimeSpawnSlot Slot;
		float Deadline = 0.f;
		/** The per-candidate breakdown is logged once per request, not on every retry tick. */
		bool bLoggedDetail = false;
	};

	/** Resolve the slot definition for one index, degrading to a ring when the DA is short. */
	FSlimeSpawnSlot ResolveSlot(const TArray<FSlimeSpawnSlot>& Slots, int32 Index, ETargetKind Kind) const;

	/**
	 * Try the primary slot and then every fallback, in order.
	 * @param bLogDetail  print one line per candidate (first attempt of a request only)
	 */
	bool TryResolveAndSpawn(FPendingSpawn& Pending, bool bLogDetail, ESlotRejectReason& OutReason);

	ASlimeEnemyBase* SpawnOne(ETargetKind Kind, const FVector& Location, const FRotator& Rotation);

	void RequestSpawn(ETargetKind Kind, const FSlimeSpawnSlot& Slot);

	void TickRetryQueue();

	void StartRetryTimer();
	void StopRetryTimer();

	UFUNCTION()
	void HandleEnemyDied(AActor* Enemy);

	/** Recompute bSupplyDone and push the state through IBattleDirector. */
	void RefreshState();

	void SetState(ESpawnPointState NewState);

	/** Capsule half height of a spawn class, read from its CDO so nothing is hard coded. */
	static float GetCapsuleHalfHeight(const TSubclassOf<ASlimeEnemyBase>& SlimeClass);

	/** From DA_SpawnLayout, filled by InitializeRuntime. */
	FSlimeSpawnPointDef Definition;

	bool bRuntimeReady = false;
	ESpawnPointState State = ESpawnPointState::AwaitingDeploy;

	int32 BatchCount = 0;
	int32 NormalPerBatch = 0;
	int32 AggroPerBatch = 0;
	int32 TotalNormalSupply = 0;
	float PlayerMinDistance = 0.f;
	float RetryWindow = 0.f;
	float RetryInterval = 0.f;
	float ActivityRadius = 0.f;
	float DefaultActivityRadius = 0.f;

	float NormalHalfHeight = 0.f;
	float AggroHalfHeight = 0.f;

	/** Batches issued so far, and whether the last one (plus its delayed spawns) is resolved. */
	int32 BatchesIssued = 0;
	bool bSupplyDone = false;

	int32 SpawnedNormalCount = 0;
	int32 SpawnedAggroCount = 0;

	TArray<TWeakObjectPtr<ASlimeEnemyBase>> LiveNormals;
	TArray<TWeakObjectPtr<ASlimeEnemyBase>> LiveAggro;

	TArray<FPendingSpawn> PendingSpawns;
	FTimerHandle RetryTimerHandle;
};
