// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeCargoVisual.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ASlimeCargoVisual::ASlimeCargoVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMesh);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Engine/BasicShapes/Cube.Cube")));
}

void ASlimeCargoVisual::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Mesh = CargoMeshAsset.LoadSynchronous())
	{
		SetVisualMesh(Mesh);
	}
	RefreshVisual();
}

void ASlimeCargoVisual::SetVisualAmount(int32 InAmount)
{
	VisualAmount = FMath::Max(1, InAmount);
	RefreshVisual();
}

void ASlimeCargoVisual::SetVisualMesh(UStaticMesh* InMesh)
{
	if (CargoMesh && InMesh)
	{
		CargoMesh->SetStaticMesh(InMesh);
	}
}

void ASlimeCargoVisual::RefreshVisual()
{
	if (!CargoMesh)
	{
		return;
	}

	const float AmountScale = 1.f + 0.12f * static_cast<float>(FMath::Max(0, VisualAmount - 1));
	CargoMesh->SetRelativeScale3D(FVector(BaseScale * AmountScale));
}
