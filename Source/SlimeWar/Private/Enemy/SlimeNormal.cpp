// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeNormal.h"

#include "Components/CapsuleComponent.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Core/SlimeWarCollisionChannels.h"
#include "Enemy/SlimeFusionComponent.h"
#include "StateTree.h"

ASlimeNormal::ASlimeNormal()
{
	TargetKind = ETargetKind::Normal;
	Mass = 1;

	// Normal slimes get their own object channel so the pairing window can drop collision with
	// exactly one partner while everything else keeps blocking (PB-10/PB-11/PB-16).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionObjectType(SlimeCollisionChannels::Fusion);
	}

	// Only normal slimes can fuse; aggressive slimes never get this component.
	Fusion = CreateDefaultSubobject<USlimeFusionComponent>(TEXT("Fusion"));

	StateTreeAsset = TSoftObjectPtr<UStateTree>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/ST_SlimeNormal.ST_SlimeNormal")));
}
