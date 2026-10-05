// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeAggro.h"

#include "Core/SlimeWarCoreTypes.h"
#include "StateTree.h"

ASlimeAggro::ASlimeAggro()
{
	TargetKind = ETargetKind::Aggressive;
	Mass = 1;

	StateTreeAsset = TSoftObjectPtr<UStateTree>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/ST_SlimeAggro.ST_SlimeAggro")));
}
