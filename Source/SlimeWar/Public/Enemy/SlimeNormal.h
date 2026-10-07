// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/SlimeEnemyBase.h"
#include "SlimeNormal.generated.h"

/**
 * Scored target. Fuses with other normal individuals inside the same spawn point (Phase B).
 */
UCLASS()
class ASlimeNormal : public ASlimeEnemyBase
{
	GENERATED_BODY()

public:
	ASlimeNormal();
};
