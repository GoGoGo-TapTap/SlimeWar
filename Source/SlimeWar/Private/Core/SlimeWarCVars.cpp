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

	int32 DebugCrosshair = 0;
	static FAutoConsoleVariableRef CVarDebugCrosshair(
		TEXT("Slime.Debug.Crosshair"),
		DebugCrosshair,
		TEXT("Draw the C++ placeholder crosshair. Default off; the UI blueprint owns the crosshair."),
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

	int32 DebugDrawEcologyState = 0;
	static FAutoConsoleVariableRef CVarDebugDrawEcologyState(
		TEXT("Slime.Debug.DrawEcologyState"),
		DebugDrawEcologyState,
		TEXT("Draw mass, health, fusion, brain, body, stun, cargo and credit state above ecology slimes. ")
		TEXT("Default off; 1 = on."),
		ECVF_Cheat);

	int32 DebugDrawAggroPath = 0;
	static FAutoConsoleVariableRef CVarDebugDrawAggroPath(
		TEXT("Slime.Debug.DrawAggroPath"),
		DebugDrawAggroPath,
		TEXT("Draw aggro slimes: cyan = planned nav path, orange trail = where it actually went, ")
		TEXT("plus a label with move status and speed. Default off."),
		ECVF_Cheat);

	int32 DebugDrawRun = 0;
	static FAutoConsoleVariableRef CVarDebugDrawRun(
		TEXT("Slime.Debug.DrawRun"),
		DebugDrawRun,
		TEXT("Draw the C++ run HUD: score, countdown, faction sites, minerals and ledger. Default off."),
		ECVF_Cheat);

	int32 DebugPaint = 1;
	static FAutoConsoleVariableRef CVarDebugPaint(
		TEXT("Slime.Debug.Paint"),
		DebugPaint,
		TEXT("Draw accumulated paint splats while the art-owned paint material is not assigned."),
		ECVF_Cheat);

	float RunTimeScale = 1.f;
	static FAutoConsoleVariableRef CVarRunTimeScale(
		TEXT("Slime.Run.TimeScale"),
		RunTimeScale,
		TEXT("Multiplier for the run timeline (1 = real time). Debug / CP-3 accelerator only."),
		ECVF_Cheat);
}
