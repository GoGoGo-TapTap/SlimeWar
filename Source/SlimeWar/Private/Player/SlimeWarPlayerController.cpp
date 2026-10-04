// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWarPlayerController.h"
#include "Debug/SlimeCheatManager.h"

ASlimeWarPlayerController::ASlimeWarPlayerController()
{
	CheatClass = USlimeCheatManager::StaticClass();
}
