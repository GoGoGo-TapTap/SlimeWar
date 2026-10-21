// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ecology/SlimeEcologyResourceTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeEcologySubsystem.generated.h"

class ASlimeEcologyEnemy;
class ASlimeFactionSite;
class ASlimeMineralPatch;
class ASlimeCargoPickup;

DECLARE_MULTICAST_DELEGATE(FSlimeEcologyExtinct);

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

	void RegisterSite(ASlimeFactionSite* Site);
	void UnregisterSite(ASlimeFactionSite* Site);
	void RegisterMineralPatch(ASlimeMineralPatch* Patch);
	void UnregisterMineralPatch(ASlimeMineralPatch* Patch);
	void RegisterCargoPickup(ASlimeCargoPickup* Pickup);
	void UnregisterCargoPickup(ASlimeCargoPickup* Pickup);

	void BeginRun();
	void EndRun();
	bool IsRunActive() const { return bRunActive; }
	bool HasEcologySites() const;

	bool TryGather(ASlimeEcologyEnemy* Enemy, ASlimeMineralPatch* Patch);
	bool TryDeliver(ASlimeEcologyEnemy* Enemy);
	bool TryPickupCargo(ASlimeEcologyEnemy* Enemy, ASlimeCargoPickup* Pickup);
	bool DebugGrantCargoFromStock(ASlimeEcologyEnemy* Enemy);
	void DropCargo(const FVector& Location, int32 Amount);
	void NotifyAgentRemoved(const ASlimeEcologyEnemy* Enemy, int32 RemovedMass);

	ASlimeMineralPatch* FindBestMineralPatch(
		const ASlimeEcologyEnemy* Enemy,
		float MaxRange,
		const TSet<TWeakObjectPtr<ASlimeMineralPatch>>* IgnoredPatches = nullptr) const;
	ASlimeCargoPickup* FindNearestCargoPickup(
		const ASlimeEcologyEnemy* Enemy,
		float MaxRange) const;
	ASlimeFactionSite* GetSiteForFaction(ESlimeFaction Faction) const;
	bool HasFactionSite(ESlimeFaction Faction) const;

	TArray<FSlimeSiteStatus> GetSiteStatuses() const;
	TArray<FSlimeMineralStatus> GetMineralStatuses() const;
	FSlimeEcologyLedger BuildLedger() const;
	bool CheckLedger(bool bLogResult, const TCHAR* Context = nullptr) const;
	int32 GetPendingSpawnCount() const { return PendingSpawns.Num(); }
	int32 GetDeliveredCargo() const { return DeliveredCargo; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	int32 GetRegisteredCount() const;
	int32 GetLiveAgentCount() const;

	int32 GetRebuildCount() const { return RebuildCount; }
	int32 GetLastQueryVisitedCells() const { return LastQueryVisitedCells; }

	FSlimeEcologyExtinct OnExtinction;

private:
	FIntPoint ToCell(const FVector& Location) const;
	void RebuildBuckets() const;
	void ProcessSpawnQueue();
	void EvaluateExtinction(float DeltaTime);
	bool SpawnOneAtSite(ASlimeFactionSite* Site);
	int32 GetLiveMass() const;
	int32 GetCarriedCargo() const;
	int32 GetGroundCargo() const;
	int32 GetPatchStock() const;
	int32 GetRegeneratedStock() const;
	ASlimeFactionSite* FindSiteForFaction(ESlimeFaction Faction) const;
	FVector ResolveSiteSpawnLocation(ASlimeFactionSite* Site) const;

	TArray<TWeakObjectPtr<ASlimeEcologyEnemy>> RegisteredAgents;
	mutable TMap<FIntPoint, TArray<TWeakObjectPtr<ASlimeEcologyEnemy>>> Buckets;
	mutable bool bBucketsDirty = true;
	mutable int32 RebuildCount = 0;
	mutable int32 LastQueryVisitedCells = 0;
	float RefreshAccumulator = 0.f;
	float RefreshInterval = 0.25f;
	float CellSize = 400.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASlimeFactionSite>> Sites;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASlimeMineralPatch>> MineralPatches;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASlimeCargoPickup>> CargoPickups;

	struct FPendingSpawn
	{
		TWeakObjectPtr<ASlimeFactionSite> Site;
	};

	TArray<FPendingSpawn> PendingSpawns;
	bool bRunActive = false;
	bool bExtinctionReported = false;
	float ExtinctionCheckAccumulator = 0.f;
	float LedgerLogAccumulator = 0.f;
	float FallProbeAccumulator = 0.f;
	int32 InitialMass = 0;
	int32 DeliveredCargo = 0;
	int32 RemovedMass = 0;
};
