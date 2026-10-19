// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeMineralPatch.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASlimeMineralPatch::ASlimeMineralPatch()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	CrystalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crystal"));
	CrystalMesh->SetupAttachment(SceneRoot);
	CrystalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		CrystalMesh->SetStaticMesh(SphereMesh.Object);
	}
	CrystalMesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	CrystalMesh->SetRelativeScale3D(FVector(0.6f));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(28.f);
}

void ASlimeMineralPatch::BeginPlay()
{
	Super::BeginPlay();

	MaxStock = FMath::Max(InitialStock, MaxStock);
	Stock = InitialStock;
	RefreshLabel();

	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->RegisterMineralPatch(this);
	}
}

void ASlimeMineralPatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Ecology || !Ecology->IsRunActive()
		|| !bRegenerates || Stock >= MaxStock || RegrowSeconds <= 0.f)
	{
		RegrowAccumulator = 0.f;
		return;
	}

	RegrowAccumulator += DeltaSeconds;
	if (RegrowAccumulator >= RegrowSeconds)
	{
		RegrowAccumulator = 0.f;
		++Stock;
		NotifyRegenerated();
		RefreshLabel();
	}
}

void ASlimeMineralPatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->UnregisterMineralPatch(this);
	}
	Super::EndPlay(EndPlayReason);
}

int32 ASlimeMineralPatch::ConsumeOne()
{
	if (Stock <= 0)
	{
		return 0;
	}

	--Stock;
	RefreshLabel();
	return 1;
}

void ASlimeMineralPatch::RefreshLabel()
{
	if (Label)
	{
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d"), *PatchId.ToString(), Stock)));
	}
}
