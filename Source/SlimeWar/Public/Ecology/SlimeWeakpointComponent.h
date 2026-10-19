// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SphereComponent.h"
#include "CoreMinimal.h"
#include "SlimeWeakpointComponent.generated.h"

/** Query-only weakpoint for mass 7/8 bodies. Positive weapon damage executes the owner. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeWeakpointComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	USlimeWeakpointComponent();

	void ConfigureForMass(int32 Mass, float BodyRadius);
	bool TryExecute(AActor* Target, AActor* Causer);
	bool IsWeakpointActive() const { return bWeakpointActive; }

private:
	bool bWeakpointActive = false;
};
