// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ecology/SlimeCargoVisual.h"
#include "SlimeCargoPickup.generated.h"

/** Neutral cargo dropped by a dead carrier. Each pickup transfers exactly one unit. */
UCLASS()
class SLIMEWAR_API ASlimeCargoPickup : public ASlimeCargoVisual
{
	GENERATED_BODY()

public:
	ASlimeCargoPickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	int32 Amount = 1;

	int32 TakeOne();
	int32 GetAmount() const { return Amount; }
	void SetAmount(int32 InAmount);
};
