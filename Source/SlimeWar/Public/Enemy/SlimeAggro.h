// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/SlimeEnemyBase.h"
#include "SlimeAggro.generated.h"

/**
 * Pressure unit: chases the player, never fuses and never awards score.
 * Stats come from DT_AggroStats (no mass tier).
 */
UCLASS()
class ASlimeAggro : public ASlimeEnemyBase
{
	GENERATED_BODY()

public:
	ASlimeAggro();
};
