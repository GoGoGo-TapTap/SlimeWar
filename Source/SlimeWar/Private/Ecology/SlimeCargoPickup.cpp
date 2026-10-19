// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeCargoPickup.h"

#include "Ecology/SlimeEcologySubsystem.h"

ASlimeCargoPickup::ASlimeCargoPickup()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASlimeCargoPickup::BeginPlay()
{
	Super::BeginPlay();
	SetVisualAmount(Amount);

	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->RegisterCargoPickup(this);
	}
}

void ASlimeCargoPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->UnregisterCargoPickup(this);
	}
	Super::EndPlay(EndPlayReason);
}

int32 ASlimeCargoPickup::TakeOne()
{
	if (Amount <= 0)
	{
		return 0;
	}

	--Amount;
	SetVisualAmount(FMath::Max(1, Amount));
	if (Amount <= 0)
	{
		Destroy();
	}
	return 1;
}

void ASlimeCargoPickup::SetAmount(int32 InAmount)
{
	Amount = FMath::Max(0, InAmount);
	SetVisualAmount(FMath::Max(1, Amount));
}
