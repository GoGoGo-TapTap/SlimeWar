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

	/** 0 = off, 1 = draw the run HUD: score, countdown and point states (Slime.Debug.DrawRun). */
	extern int32 DebugDrawRun;

	/**
	 * Multiplier for the run timeline (Slime.Run.TimeScale). 1 = real time.
	 *
	 * Debug / CP-3 accelerator only: it changes how fast the 180 s run plays out, never a data value.
	 * Set it to 0 to freeze the timeline while inspecting something.
	 */
	extern float RunTimeScale;
}
