// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "SlimeCheatManager.generated.h"

/**
 * Phase 0 cheat commands used by the CP-0 acceptance pass.
 * Type these into the PIE console (backtick).
 */
UCLASS()
class USlimeCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** Print the loaded data tables, their paths and row names. */
	UFUNCTION(Exec)
	void SlimeDumpTables();

	/** Re-read the tables after editing DA_RunConfig / the DataTables. */
	UFUNCTION(Exec)
	void SlimeReloadTables();

	/** Apply Amount damage to the closest live enemy through the unified damage entry. */
	UFUNCTION(Exec)
	void SlimeDamageNearestEnemy(float Amount);

	// -- Phase A: CP-1 needs enemies before the Phase C spawn points exist. --

	/** Spawn Count normal slimes in front of the player. */
	UFUNCTION(Exec)
	void SlimeSpawnNormal(int32 Count = 1);

	/** Spawn Count aggressive slimes in front of the player. */
	UFUNCTION(Exec)
	void SlimeSpawnAggro(int32 Count = 1);

	/** Destroy every live slime. */
	UFUNCTION(Exec)
	void SlimeClearEnemies();

	/** Kill the player through the normal damage path (for death-path testing). */
	UFUNCTION(Exec)
	void SlimeKillPlayer();

	// -- Phase B: fusion scenarios for the CP-2 pass --

	/** Spawn Count normal slimes of a given mass in a tight cluster (they pair up on their own). */
	UFUNCTION(Exec)
	void SlimeSpawnNormalAtMass(int32 Mass = 1, int32 Count = 1);

	/** Re-stat the nearest normal slime to Mass, keeping its health ratio. */
	UFUNCTION(Exec)
	void SlimeSetMass(int32 Mass);

	/** Force the two nearest normal slimes of one spawn point into a fusion pair. */
	UFUNCTION(Exec)
	void SlimeForceFuse();

	// -- Phase C: run flow (PC-06 / CP-3) --

	/** Start (or restart) the run without waiting for the auto-start. */
	UFUNCTION(Exec)
	void SlimeRunStart();

	/** End the run as a time-up. The player-death path is exercised by SlimeKillPlayer. */
	UFUNCTION(Exec)
	void SlimeRunEnd();

	/** Multiplier for the run timeline: 10 reaches the last batch in ~10 s. 1 = real time. */
	UFUNCTION(Exec)
	void SlimeRunTimeScale(float Scale);

	/** Print every number the CP-3 hand calculation needs (score per mass tier, point states). */
	UFUNCTION(Exec)
	void SlimeRunStatus();
};
