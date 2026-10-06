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

	/** 0 = off, 1 = draw fusion pairs, contact progress and meeting points (Slime.Debug.DrawFusion). */
	extern int32 DebugDrawFusion;

	/**
	 * 0 = off, 1 = draw each aggro slime's planned nav path plus the trail it actually walked
	 * (Slime.Debug.DrawAggroPath). Default on: the planned path cannot show crowd avoidance, the
	 * trail is what makes "did it route around?" readable.
	 */
	extern int32 DebugDrawAggroPath;
}
