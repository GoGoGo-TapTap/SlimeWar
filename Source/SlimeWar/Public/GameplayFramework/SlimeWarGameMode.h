// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/BattleDirectorInterface.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/GameModeBase.h"
#include "SlimeWarGameMode.generated.h"

/**
 * Broadcast surface of the battle director (Phase C).
 *
 * The run subsystem and the score subsystem subscribe to these instead of having the GameMode
 * call into Flow directly: GameplayFramework is a lower layer than Flow, so the dependency must
 * point this way (plan section 3.1 / rule 3). It is also what keeps this class a thin forwarder
 * (plan section 4.3, risk 7) instead of turning into the assembly point of the whole game.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeEnemyKilledEvent, ETargetKind /*Kind*/, int32 /*Mass*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimeEnemyFusedEvent, int32 /*ResultMass*/, const FVector& /*Location*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlimePointStateChangedEvent, int32 /*PointId*/, ESpawnPointState /*NewState*/);
DECLARE_MULTICAST_DELEGATE(FSlimePlayerDiedEvent);

/**
 * The battle director. Every IBattleDirector callback is logged and then broadcast; the actual
 * run logic lives in URunSubsystem / UScoreSubsystem on the Flow side.
 */
UCLASS(minimalapi, config = Game)
class ASlimeWarGameMode : public AGameModeBase, public IBattleDirector
{
	GENERATED_BODY()

public:
	ASlimeWarGameMode();

	virtual void PreInitializeComponents() override;

	//~ Begin IBattleDirector
	virtual void OnEnemyKilled(ETargetKind Kind, int32 Mass) override;
	virtual void OnEnemyFused(int32 ResultMass, const FVector& Location) override;
	virtual void OnPointStateChanged(int32 PointId, ESpawnPointState NewState) override;
	virtual void OnPlayerDied() override;
	//~ End IBattleDirector

	FSlimeEnemyKilledEvent OnEnemyKilledEvent;
	FSlimeEnemyFusedEvent OnEnemyFusedEvent;
	FSlimePointStateChangedEvent OnPointStateChangedEvent;
	FSlimePlayerDiedEvent OnPlayerDiedEvent;

protected:
	/**
	 * GameState class, as a soft reference so this class never has to include a Flow header.
	 *
	 * It has to be applied in PreInitializeComponents: AGameModeBase hardcodes GameStateClass to
	 * AGameStateBase in its constructor and spawns the GameState here, so this is the only point
	 * where the override can still take effect. Set in DefaultGame.ini.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Run")
	TSoftClassPtr<AGameStateBase> RunGameStateClass;
};

/** Accessor for the battle director. Logs instead of crashing when the context is wrong. */
SLIMEWAR_API ASlimeWarGameMode* GetSlimeGameMode(const UObject* WorldContextObject);
