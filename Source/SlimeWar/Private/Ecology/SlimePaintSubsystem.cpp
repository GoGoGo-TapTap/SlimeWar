// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimePaintSubsystem.h"

#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimePaintField.h"
#include "Components/DecalComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void USlimePaintSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this))
	{
		WorldSize = Rules->PaintWorldSize;
	}
}

void USlimePaintSubsystem::Deinitialize()
{
	ResetPaint();
	SplatRecords.Reset();
	PaintRenderTarget = nullptr;
	SplatMID = nullptr;
	Super::Deinitialize();
}

void USlimePaintSubsystem::EnsureRenderTarget()
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (PaintRenderTarget || !Rules)
	{
		return;
	}

	PaintRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("SlimePaintRT"));
	PaintRenderTarget->RenderTargetFormat = RTF_RGBA8;
	PaintRenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 0.f);
	PaintRenderTarget->InitAutoFormat(
		FMath::Max(64, Rules->PaintTextureWidth),
		FMath::Max(64, Rules->PaintTextureHeight));
	PaintRenderTarget->UpdateResourceImmediate(false);
	UKismetRenderingLibrary::ClearRenderTarget2D(
		this,
		PaintRenderTarget,
		FLinearColor::Transparent);
}

void USlimePaintSubsystem::BeginRun()
{
	EnsureRenderTarget();
	ResetPaint();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimePaintField> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ASlimePaintField>(
		ASlimePaintField::StaticClass(),
		FVector(WorldCentre.X, WorldCentre.Y, 2.f),
		FRotator::ZeroRotator,
		Params);
}

void USlimePaintSubsystem::ResetPaint()
{
	for (const TWeakObjectPtr<UDecalComponent>& Decal : PaintDecals)
	{
		if (UDecalComponent* DecalComponent = Decal.Get())
		{
			DecalComponent->DestroyComponent();
		}
	}
	PaintDecals.Reset();

	SplatRecords.Reset();
	SplatCount = 0;
	if (PaintRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(
			this,
			PaintRenderTarget,
			FLinearColor::Transparent);
	}
}

void USlimePaintSubsystem::ConfigureWorldBounds(
	const FVector& Centre,
	const FVector2D& Size)
{
	WorldCentre = Centre;
	if (Size.X > 0.f && Size.Y > 0.f)
	{
		WorldSize = Size;
	}
}

void USlimePaintSubsystem::RegisterSplat(
	const FVector& Location,
	const FLinearColor& Color,
	float Radius,
	float Rotation)
{
	EnsureRenderTarget();
	FSlimePaintSplat Splat;
	Splat.Location = Location;
	Splat.Color = Color;
	Splat.Radius = FMath::Max(1.f, Radius);
	Splat.Rotation = Rotation;

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (Rules && SplatRecords.Num() < Rules->MaxPaintSplatRecords)
	{
		SplatRecords.Add(Splat);
	}
	++SplatCount;

	const USlimeEcologyRules* DefaultRules = GetDefault<USlimeEcologyRules>();
	const TSoftObjectPtr<UMaterialInterface>& DecalAsset =
		(Rules && !Rules->PaintDecalMaterial.IsNull())
			? Rules->PaintDecalMaterial
			: DefaultRules->PaintDecalMaterial;
	if (UMaterialInterface* DecalMaterial = DecalAsset.LoadSynchronous())
	{
		SpawnPaintDecal(Splat, DecalMaterial);
	}
	else
	{
		DrawSplatToRenderTarget(Splat);
	}
}

FLinearColor USlimePaintSubsystem::GetFactionColor(ESlimeFaction Faction) const
{
	if (const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this))
	{
		FLinearColor Color = Rules->GetFactionColorOrDefault(Faction);
		Color.A = 0.9f;
		return Color;
	}

	return FLinearColor::White;
}

void USlimePaintSubsystem::SpawnPaintDecal(
	const FSlimePaintSplat& Splat,
	UMaterialInterface* DecalMaterial)
{
	UWorld* World = GetWorld();
	if (!World || !DecalMaterial)
	{
		return;
	}

	const FVector DecalSize(
		FMath::Max(1.f, Splat.Radius * 0.5f),
		FMath::Max(1.f, Splat.Radius * 2.f),
		FMath::Max(1.f, Splat.Radius * 2.f));
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		this,
		DecalMaterial,
		DecalSize,
		Splat.Location + FVector(0.f, 0.f, 2.f),
		FRotator(-90.f, Splat.Rotation, 0.f),
		0.f);
	if (Decal)
	{
		PaintDecals.Add(Decal);
	}
}

void USlimePaintSubsystem::DrawSplatToRenderTarget(const FSlimePaintSplat& Splat)
{
	USlimeEcologyRules* Rules = const_cast<USlimeEcologyRules*>(USlimeEcologyRules::GetFor(this));
	UWorld* World = GetWorld();
	if (!Rules || !World || !PaintRenderTarget || Rules->PaintSplatMaterial.IsNull())
	{
		return;
	}

	if (!SplatMID)
	{
		if (UMaterialInterface* Material = Rules->PaintSplatMaterial.LoadSynchronous())
		{
			SplatMID = UMaterialInstanceDynamic::Create(Material, this);
		}
	}
	if (!SplatMID)
	{
		return;
	}

	const FVector2D Min = FVector2D(WorldCentre.X, WorldCentre.Y) - WorldSize * 0.5f;
	const FVector2D UV(
		(Splat.Location.X - Min.X) / WorldSize.X,
		(Splat.Location.Y - Min.Y) / WorldSize.Y);
	SplatMID->SetVectorParameterValue(Rules->PaintSplatUVParameter, FLinearColor(UV.X, UV.Y, 0.f, 0.f));
	SplatMID->SetVectorParameterValue(Rules->PaintSplatColorParameter, Splat.Color);
	SplatMID->SetScalarParameterValue(
		Rules->PaintSplatRadiusParameter,
		Splat.Radius / FMath::Max(1.f, WorldSize.X));
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, PaintRenderTarget, SplatMID);
}
