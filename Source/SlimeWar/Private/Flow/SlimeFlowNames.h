// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"

/** Human readable names for logs and the debug HUD (single source, so they never drift apart). */
namespace SlimeFlowNames
{
	inline const TCHAR* RunState(ESlimeRunState State)
	{
		switch (State)
		{
		case ESlimeRunState::Idle:		return TEXT("Idle");
		case ESlimeRunState::Deploying:	return TEXT("Deploying");
		case ESlimeRunState::Running:	return TEXT("Running");
		case ESlimeRunState::Result:	return TEXT("Result");
		case ESlimeRunState::Ended:		return TEXT("Ended");
		default:						return TEXT("?");
		}
	}

	inline const TCHAR* EndReason(ERunEndReason Reason)
	{
		switch (Reason)
		{
		case ERunEndReason::TimeUp:		return TEXT("TimeUp");
		case ERunEndReason::PlayerDied:	return TEXT("PlayerDied");
		default:						return TEXT("None");
		}
	}

	inline const TCHAR* PointState(ESpawnPointState State)
	{
		switch (State)
		{
		case ESpawnPointState::AwaitingDeploy:		return TEXT("AwaitingDeploy");
		case ESpawnPointState::Spawning:			return TEXT("Spawning");
		case ESpawnPointState::DepletedNotCleared:	return TEXT("DepletedNotCleared");
		case ESpawnPointState::Cleared:				return TEXT("Cleared");
		default:									return TEXT("?");
		}
	}
}
