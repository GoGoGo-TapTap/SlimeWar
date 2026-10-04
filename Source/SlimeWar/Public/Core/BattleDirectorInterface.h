// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/SlimeWarCoreTypes.h"
#include "BattleDirectorInterface.generated.h"

/**
 * The single cross-module contract (plan red line C18).
 * Enemy / Weapon / UI never know about score systems, they only know this interface.
 * No GAS type may ever appear in this header.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBattleDirector : public UInterface
{
	GENERATED_BODY()
};

class IBattleDirector
{
	GENERATED_BODY()

public:
	/** A normal target died. The only scoring entry point. */
	virtual void OnEnemyKilled(ETargetKind Kind, int32 Mass) = 0;

	/** Two slimes fused successfully. Statistics / presentation only, never score. */
	virtual void OnEnemyFused(int32 ResultMass) = 0;

	/** A spawn point changed state. Drives HUD markers. */
	virtual void OnPointStateChanged(int32 PointId, ESpawnPointState NewState) = 0;

	/** The player died. Immediate run over (failure). */
	virtual void OnPlayerDied() = 0;
};
