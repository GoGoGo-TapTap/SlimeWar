// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/SlimeWarGameMode.h"
#include "Core/SlimeWarLog.h"
#include "Debug/SlimeHUD.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
// TSoftClassPtr<AGameStateBase>::LoadSynchronous needs the complete type (GameModeBase.h only
// forward declares it).
#include "GameFramework/GameStateBase.h"
#include "Player/SlimeWarPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ASlimeWarGameMode::ASlimeWarGameMode()
{
	// Keep using the template character blueprint (which derives from ASlimeWarCharacter).
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	// Routes console input to USlimeCheatManager (SlimeDumpTables / SlimeDamageNearestEnemy).
	PlayerControllerClass = ASlimeWarPlayerController::StaticClass();

	// C++ crosshair + Phase A debug draw (no widget assets needed).
	HUDClass = ASlimeHUD::StaticClass();
}

void ASlimeWarGameMode::PreInitializeComponents()
{
	// AGameModeBase spawns the GameState in this call and its GameStateClass is hardcoded in the base
	// constructor, so a config driven override has to happen right here, before Super.
	if (UClass* ConfiguredGameStateClass = RunGameStateClass.LoadSynchronous())
	{
		GameStateClass = ConfiguredGameStateClass;
	}

	Super::PreInitializeComponents();
}

void ASlimeWarGameMode::OnEnemyKilled(ETargetKind Kind, int32 Mass)
{
	// Design 4.6.3: aggressive individuals never award score. The filter lives in the single
	// scoring entry point, so Phase C's score subsystem cannot pick them up by accident.
	if (Kind != ETargetKind::Normal)
	{
		UE_LOG(LogSlimeWar, Verbose,
			TEXT("[BattleDirector] OnEnemyKilled: aggressive target (mass %d), no score."), Mass);
		return;
	}

	UE_LOG(LogSlimeWar, Verbose, TEXT("[BattleDirector] OnEnemyKilled: Kind=%d Mass=%d."),
		static_cast<int32>(Kind), Mass);

	OnEnemyKilledEvent.Broadcast(Kind, Mass);
}

void ASlimeWarGameMode::OnEnemyFused(int32 ResultMass)
{
	UE_LOG(LogSlimeWar, Verbose, TEXT("[BattleDirector] OnEnemyFused: ResultMass=%d (statistics only, never score)."),
		ResultMass);

	OnEnemyFusedEvent.Broadcast(ResultMass);
}

void ASlimeWarGameMode::OnPointStateChanged(int32 PointId, ESpawnPointState NewState)
{
	UE_LOG(LogSlimeWar, Verbose, TEXT("[BattleDirector] OnPointStateChanged: PointId=%d NewState=%d."),
		PointId, static_cast<int32>(NewState));

	OnPointStateChangedEvent.Broadcast(PointId, NewState);
}

void ASlimeWarGameMode::OnPlayerDied()
{
	UE_LOG(LogSlimeWar, Log, TEXT("[BattleDirector] OnPlayerDied: the run ends now."));

	OnPlayerDiedEvent.Broadcast();
}

ASlimeWarGameMode* GetSlimeGameMode(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GetSlimeGameMode: invalid world context."));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GetSlimeGameMode: could not resolve a world from the context."));
		return nullptr;
	}

	ASlimeWarGameMode* GameMode = World->GetAuthGameMode<ASlimeWarGameMode>();
	if (!GameMode)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("GetSlimeGameMode: world %s has no ASlimeWarGameMode."), *World->GetName());
	}

	return GameMode;
}
