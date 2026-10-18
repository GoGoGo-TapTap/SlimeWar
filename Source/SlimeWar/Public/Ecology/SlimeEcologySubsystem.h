// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeEcologySubsystem.generated.h"

class ASlimeEcologyEnemy;

/**
 * Registry and spatial hash for ecology agents. Rebuilding is interval based; individual brains
 * query this subsystem instead of iterating every slime in the world.
 */
UCLASS()
class SLIMEWAR_API USlimeEcologySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void RegisterAgent(ASlimeEcologyEnemy* Agent);
	void UnregisterAgent(ASlimeEcologyEnemy* Agent);

	void QueryNeighbors(
		const ASlimeEcologyEnemy* Source,
		float Radius,
		TArray<ASlimeEcologyEnemy*>& OutNeighbors) const;

	void QueryAgentsAroundLocation(
		const FVector& Location,
		float Radius,
		TArray<ASlimeEcologyEnemy*>& OutNeighbors) const;

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	int32 GetRegisteredCount() const;

	int32 GetRebuildCount() const { return RebuildCount; }
	int32 GetLastQueryVisitedCells() const { return LastQueryVisitedCells; }

private:
	FIntPoint ToCell(const FVector& Location) const;
	void RebuildBuckets() const;

	TArray<TWeakObjectPtr<ASlimeEcologyEnemy>> RegisteredAgents;
	mutable TMap<FIntPoint, TArray<TWeakObjectPtr<ASlimeEcologyEnemy>>> Buckets;
	mutable bool bBucketsDirty = true;
	mutable int32 RebuildCount = 0;
	mutable int32 LastQueryVisitedCells = 0;
	float RefreshAccumulator = 0.f;
	float RefreshInterval = 0.25f;
	float CellSize = 400.f;
};
