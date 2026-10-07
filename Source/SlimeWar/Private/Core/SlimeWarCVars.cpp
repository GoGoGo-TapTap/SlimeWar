// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/SlimeWarCVars.h"
#include "HAL/IConsoleManager.h"

namespace SlimeCVars
{
	int32 DebugCombatLog = 0;
	static FAutoConsoleVariableRef CVarDebugCombatLog(
		TEXT("Slime.Debug.CombatLog"),
		DebugCombatLog,
		TEXT("Log every damage and death step through USlimeCombatSubsystem / USlimeHealthComponent."),
		ECVF_Cheat);

	int32 DebugDrawEnemyState = 0;
	static FAutoConsoleVariableRef CVarDebugDrawEnemyState(
		TEXT("Slime.Debug.DrawEnemyState"),
		DebugDrawEnemyState,
		TEXT("Draw enemy state tags, activity radius and fusion target links above each enemy."),
		ECVF_Cheat);

	int32 DebugCrosshair = 1;
	static FAutoConsoleVariableRef CVarDebugCrosshair(
		TEXT("Slime.Debug.Crosshair"),
		DebugCrosshair,
		TEXT("Draw the placeholder crosshair until the Phase D HUD replaces it. 0 = off."),
		ECVF_Cheat);

	int32 DebugDrawAimAssist = 0;
	static FAutoConsoleVariableRef CVarDebugDrawAimAssist(
		TEXT("Slime.Debug.DrawAimAssist"),
		DebugDrawAimAssist,
		TEXT("Draw the camera ray, the assisted fire ray and the target aim assist pulled toward."),
		ECVF_Cheat);

	int32 DebugDrawFusion = 0;
	static FAutoConsoleVariableRef CVarDebugDrawFusion(
		TEXT("Slime.Debug.DrawFusion"),
		DebugDrawFusion,
		TEXT("Draw fusion pairs (colour = state), contact progress, meeting points and cooldowns."),
		ECVF_Cheat);

	int32 DebugDrawAggroPath = 1;
	static FAutoConsoleVariableRef CVarDebugDrawAggroPath(
		TEXT("Slime.Debug.DrawAggroPath"),
		DebugDrawAggroPath,
		TEXT("Draw aggro slimes: cyan = planned nav path, orange trail = where it actually went, ")
		TEXT("plus a label with move status and speed."),
		ECVF_Cheat);

	int32 DebugDrawRun = 1;
	static FAutoConsoleVariableRef CVarDebugDrawRun(
		TEXT("Slime.Debug.DrawRun"),
		DebugDrawRun,
		TEXT("Draw the Phase C run HUD: score, countdown, point states and live enemy counts. 0 = off."),
		ECVF_Cheat);

	float RunTimeScale = 1.f;
	static FAutoConsoleVariableRef CVarRunTimeScale(
		TEXT("Slime.Run.TimeScale"),
		RunTimeScale,
		TEXT("Multiplier for the run timeline (1 = real time). Debug / CP-3 accelerator only."),
		ECVF_Cheat);
}
