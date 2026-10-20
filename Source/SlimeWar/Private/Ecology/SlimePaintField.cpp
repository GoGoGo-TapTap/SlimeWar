// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimePaintField.h"

#include "Components/StaticMeshComponent.h"
#include "Core/SlimeWarCVars.h"
#include "DrawDebugHelpers.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimePaintSubsystem.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASlimePaintField::ASlimePaintField()
{
	PrimaryActorTick.bCanEverTick = true;

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	SetRootComponent(Surface);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Surface->SetStaticMesh(PlaneMesh.Object);
	}
}

USlimePaintSubsystem* ASlimePaintField::GetPaintSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USlimePaintSubsystem>() : nullptr;
}

void ASlimePaintField::BeginPlay()
{
	Super::BeginPlay();

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	USlimePaintSubsystem* Paint = GetPaintSubsystem();
	if (!Paint)
	{
		return;
	}

	if (WorldSize.X <= 0.f || WorldSize.Y <= 0.f)
	{
		WorldSize = Rules ? Rules->PaintWorldSize : FVector2D(4400.f, 3000.f);
	}
	Paint->ConfigureWorldBounds(GetActorLocation(), WorldSize);
	Paint->BeginRun();
	Surface->SetRelativeScale3D(FVector(WorldSize.X / 100.f, WorldSize.Y / 100.f, 1.f));

	UMaterialInterface* Material = PaintMaterial;
	if (!Material && Rules)
	{
		Material = Rules->PaintSurfaceMaterial.LoadSynchronous();
	}
	if (!Material)
	{
		// Decal-based paint uses the level's own ground meshes; a default-material
		// plane would cover the floor with the engine checker/grid material.
		Surface->SetVisibility(false);
		return;
	}

	if (Material && Paint->GetPaintRenderTarget())
	{
		SurfaceMID = UMaterialInstanceDynamic::Create(Material, this);
		SurfaceMID->SetTextureParameterValue(
			Rules ? Rules->PaintTextureParameter : FName(TEXT("SlimePaint")),
			Paint->GetPaintRenderTarget());
		Surface->SetMaterial(0, SurfaceMID);
	}
}

void ASlimePaintField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const USlimePaintSubsystem* Paint = GetPaintSubsystem();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Paint || !Rules || SlimeCVars::DebugPaint == 0 || SurfaceMID)
	{
		return;
	}

	for (const FSlimePaintSplat& Splat : Paint->GetSplatRecords())
	{
		DrawDebugCircle(
			GetWorld(),
			FVector(Splat.Location.X, Splat.Location.Y, GetActorLocation().Z + 2.f),
			Splat.Radius,
			24,
			Splat.Color.ToFColor(true),
			false,
			0.05f,
			0,
			2.f,
			FVector::ForwardVector,
			FVector::RightVector,
			false);
	}
}
