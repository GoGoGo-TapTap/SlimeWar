// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/SlimeWarGameMode.h"
#include "Core/SlimeWarLog.h"
#include "Debug/SlimeHUD.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

void ASlimeWarGameMode::OnEnemyKilled(ETargetKind Kind, int32 Mass)
{
	// TODO(Phase C): forward to UScoreSubsystem. Phase 0 only logs so the path is verifiable.
	UE_LOG(LogSlimeWar, Log, TEXT("[BattleDirector] OnEnemyKilled: Kind=%d Mass=%d (scoring not implemented yet)"),
		static_cast<int32>(Kind), Mass);
}

void ASlimeWarGameMode::OnEnemyFused(int32 ResultMass)
{
	UE_LOG(LogSlimeWar, Log, TEXT("[BattleDirector] OnEnemyFused: ResultMass=%d (statistics only, never score)"), ResultMass);
}

void ASlimeWarGameMode::OnPointStateChanged(int32 PointId, ESpawnPointState NewState)
{
	UE_LOG(LogSlimeWar, Log, TEXT("[BattleDirector] OnPointStateChanged: PointId=%d NewState=%d"),
		PointId, static_cast<int32>(NewState));
}

void ASlimeWarGameMode::OnPlayerDied()
{
	UE_LOG(LogSlimeWar, Log, TEXT("[BattleDirector] OnPlayerDied: run over (TimeUp=0, PlayerDied=1)"));
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
