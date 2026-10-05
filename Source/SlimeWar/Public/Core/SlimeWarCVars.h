// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Project console variables. All of them use the "Slime." prefix. */
namespace SlimeCVars
{
	/** 0 = off, 1 = log every damage / death step (Slime.Debug.CombatLog). */
	extern int32 DebugCombatLog;

	/** 0 = off, 1 = debug draw enemy state tags and activity radius (Slime.Debug.DrawEnemyState). */
	extern int32 DebugDrawEnemyState;

	/** 0 = off, 1 = draw the crosshair (Slime.Debug.Crosshair). Default on. */
	extern int32 DebugCrosshair;

	/** 0 = off, 1 = draw the camera ray / assisted ray / picked target (Slime.Debug.DrawAimAssist). */
	extern int32 DebugDrawAimAssist;
}
