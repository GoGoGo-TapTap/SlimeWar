// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyEnemy.h"

#include "Ecology/SlimeEcologyAIController.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Engine/World.h"

ASlimeEcologyEnemy::ASlimeEcologyEnemy()
{
	TargetKind = ETargetKind::Normal;
	Mass = 1;
	AIControllerClass = ASlimeEcologyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Brain = CreateDefaultSubobject<USlimeEcologyBrainComponent>(TEXT("EcologyBrain"));
	Locomotion = CreateDefaultSubobject<USlimeLocomotionComponent>(TEXT("Locomotion"));
	BodyInteraction = CreateDefaultSubobject<USlimeBodyInteractionComponent>(TEXT("BodyInteraction"));
}

void ASlimeEcologyEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (Brain)
	{
		Brain->SetHome(GetActivityCenter(), GetActivityRadius());
	}

	if (UWorld* World = GetWorld())
	{
		if (USlimeEcologySubsystem* Ecology = World->GetSubsystem<USlimeEcologySubsystem>())
		{
			Ecology->RegisterAgent(this);
		}
	}
}

void ASlimeEcologyEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (USlimeEcologySubsystem* Ecology = World->GetSubsystem<USlimeEcologySubsystem>())
		{
			Ecology->UnregisterAgent(this);
		}
	}

	if (Locomotion)
	{
		Locomotion->ClearMoveGoal();
	}

	Super::EndPlay(EndPlayReason);
}

void ASlimeEcologyEnemy::InitializeEcology(
	ESlimeFaction InFaction,
	int32 InPointId,
	const FVector& InActivityCenter,
	float InActivityRadius)
{
	SetFaction(InFaction);
	InitializeFromSpawn(InPointId, InActivityCenter, InActivityRadius);

	if (Brain)
	{
		Brain->SetHome(InActivityCenter, InActivityRadius);
		Brain->ForceRefresh();
	}
}

void ASlimeEcologyEnemy::SetFaction(ESlimeFaction InFaction)
{
	Faction = InFaction;
}

void ASlimeEcologyEnemy::IgnorePlayerForMovement()
{
	// New ecology bodies intentionally keep player collision. The legacy base implementation is
	// for the old fusion prototype only.
}
