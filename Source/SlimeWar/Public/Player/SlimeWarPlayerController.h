// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SlimeWarPlayerController.generated.h"

class IBattleDirector;

/**
 * Player controller shell. Phase 0 only wires the cheat manager so the CP-0 acceptance
 * commands (SlimeDumpTables / SlimeDamageNearestEnemy) are reachable in PIE.
 */
UCLASS()
class ASlimeWarPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASlimeWarPlayerController();
};
