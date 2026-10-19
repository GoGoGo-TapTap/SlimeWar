// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeFactionSite.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASlimeFactionSite::ASlimeFactionSite()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CylinderMesh.Object);
	}
	PadMesh->SetRelativeScale3D(FVector(3.f, 3.f, 0.04f));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(55.f);
	Label->SetTextRenderColor(FColor::White);
}

void ASlimeFactionSite::BeginPlay()
{
	Super::BeginPlay();

	Label->SetText(FText::FromName(SiteId));
	Label->SetTextRenderColor(DisplayColor.ToFColor(true));

	if (UMaterialInstanceDynamic* Material =
		PadMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), DisplayColor);
	}

	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->RegisterSite(this);
	}
}

void ASlimeFactionSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->UnregisterSite(this);
	}
	Super::EndPlay(EndPlayReason);
}
