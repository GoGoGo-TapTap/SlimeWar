// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StatTableProvider.generated.h"

class USlimeRunConfig;
class UDataTable;

/**
 * The single lookup service for slime and weapon stats.
 *
 * Tables are referenced from USlimeGameSettings -> USlimeRunConfig and are cached on
 * Initialize. A missing row never returns garbage: it logs an error and hands back the
 * struct default (degraded, but visible).
 */
UCLASS()
class UStatTableProvider : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Look up by mass (row name "1".."8"). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Data")
	bool GetSlimeStat(int32 Mass, FSlimeStatRow& Out) const;

	/** Look up by weapon id (row name, e.g. "Rifle"). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Data")
	bool GetWeaponStat(FName WeaponId, FWeaponStatRow& Out) const;

	/** Look up the aggressive slime row (row name comes from USlimeRunConfig::AggroRowName). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Data")
	bool GetAggroStat(FSlimeAggroStatRow& Out) const;

	/** Dump table paths and row names to the log. Used by the CP-0 acceptance check. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Data")
	void DumpLoadedTables() const;

	UFUNCTION(BlueprintCallable, Category = "Slime|Data")
	bool ReloadTables();

	UFUNCTION(BlueprintPure, Category = "Slime|Data")
	const USlimeRunConfig* GetRunConfig() const { return RunConfig; }

protected:
	bool LoadTables();

	UPROPERTY(Transient)
	TObjectPtr<USlimeRunConfig> RunConfig = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SlimeStatTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> WeaponStatTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AggroStatTable = nullptr;
};
