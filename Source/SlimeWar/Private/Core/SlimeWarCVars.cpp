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
}
