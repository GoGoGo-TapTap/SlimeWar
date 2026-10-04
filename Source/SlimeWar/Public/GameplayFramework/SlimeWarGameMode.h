// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BattleDirectorInterface.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/GameModeBase.h"
#include "SlimeWarGameMode.generated.h"

/**
 * The battle director. Phase 0 only logs every callback: the real logic arrives with
 * URRunSubsystem / UScoreSubsystem so that this class stays a thin forwarder (plan rule 2).
 */
UCLASS(minimalapi)
class ASlimeWarGameMode : public AGameModeBase, public IBattleDirector
{
	GENERATED_BODY()

public:
	ASlimeWarGameMode();

	//~ Begin IBattleDirector
	virtual void OnEnemyKilled(ETargetKind Kind, int32 Mass) override;
	virtual void OnEnemyFused(int32 ResultMass) override;
	virtual void OnPointStateChanged(int32 PointId, ESpawnPointState NewState) override;
	virtual void OnPlayerDied() override;
	//~ End IBattleDirector
};

/** Accessor for the battle director. Logs instead of crashing when the context is wrong. */
SLIMEWAR_API ASlimeWarGameMode* GetSlimeGameMode(const UObject* WorldContextObject);
