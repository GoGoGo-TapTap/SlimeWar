// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeEnemyManagerSubsystem.generated.h"

class ASlimeEnemyBase;

/**
 * The single spawn entry for enemies (PC-08, light weight variant).
 *
 * Phase C deliberately does NOT pool actors: the peak-180 cost of this game is 180 agents being
 * alive at the same time, which pooling does not reduce, while pooling an ACharacter with a
 * StateTree, a CrowdFollowingComponent and a fusion component is a large amount of reset logic.
 * Instead this subsystem centralises spawning and keeps the live/peak counters that PE-05 and the
 * debug HUD need. If the Phase E stress test shows spawn/destroy cost, pooling lands here.
 */
UCLASS()
class USlimeEnemyManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context is not usable. */
	static USlimeEnemyManagerSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Spawn one slime and register it for bookkeeping.
	 *
	 * @param ActivityCenter Centre of the activity radius the normal slime is allowed to roam
	 *                       (the spawn point centre, not the slot position).
	 * @param ActivityRadius cm; radius the normal slime must stay inside. 0 = let the AI fall back
	 *                       to the global USlimeRunConfig::AIActivityRadius.
	 * @return the spawned enemy, or null when the class is unset or the spawn failed.
	 */
	ASlimeEnemyBase* SpawnEnemy(
		TSubclassOf<ASlimeEnemyBase> SlimeClass,
		int32 PointId,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& ActivityCenter,
		float ActivityRadius = 0.f);

	/** Destroy every registered slime (Phase D retry, debug commands). */
	int32 DespawnAllEnemies();

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetLiveEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetLiveCount(ETargetKind Kind) const;

	UFUNCTION(BlueprintPure, Category = "Slime|Flow")
	int32 GetPeakLiveEnemyCount() const { return PeakLiveEnemyCount; }

protected:
	virtual void Deinitialize() override;

	UFUNCTION()
	void HandleEnemyDied(AActor* Enemy);

private:
	void RemoveLiveEnemy(const ASlimeEnemyBase* Enemy);

	/** Spawned enemies. Weak so a destroyed actor never keeps the list alive. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ASlimeEnemyBase>> LiveEnemies;

	int32 PeakLiveEnemyCount = 0;
};
