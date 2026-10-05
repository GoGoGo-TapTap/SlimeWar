// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeNormal.h"

#include "Core/SlimeWarCoreTypes.h"
#include "StateTree.h"

ASlimeNormal::ASlimeNormal()
{
	TargetKind = ETargetKind::Normal;
	Mass = 1;

	StateTreeAsset = TSoftObjectPtr<UStateTree>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/ST_SlimeNormal.ST_SlimeNormal")));
}
