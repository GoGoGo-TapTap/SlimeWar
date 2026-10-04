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
		TEXT("Draw enemy state tags and activity radius."),
		ECVF_Cheat);
}
