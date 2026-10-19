// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "SlimeEcologyResourceTypes.generated.h"

USTRUCT(BlueprintType)
struct FSlimeSiteStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	FName SiteId;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	ESlimeFaction Faction = ESlimeFaction::A;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 LiveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 LiveMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 DeliveredCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 CoreCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 PendingSpawns = 0;
};

USTRUCT(BlueprintType)
struct FSlimeMineralStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	FName PatchId;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 Stock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 InitialStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 MaxStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	bool bRegenerates = false;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	float RegrowRemaining = 0.f;
};

USTRUCT(BlueprintType)
struct FSlimeEcologyLedger
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 InitialStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 RegeneratedStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 StockInPatches = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 CargoCarried = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 CargoOnGround = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 DeliveredCargo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 InitialMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 DeliveredMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 LiveMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Slime|Ecology")
	int32 RemovedMass = 0;

	bool IsResourceBalanced() const
	{
		return StockInPatches + CargoCarried + CargoOnGround + DeliveredCargo
			== InitialStock + RegeneratedStock;
	}

	bool IsMassBalanced() const
	{
		return InitialMass + DeliveredMass == LiveMass + RemovedMass;
	}
};
