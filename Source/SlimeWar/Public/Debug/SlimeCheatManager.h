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
};
