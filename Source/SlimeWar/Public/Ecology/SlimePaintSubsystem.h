// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimePaintSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureRenderTarget2D;
class UDecalComponent;

struct FSlimePaintSplat
{
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Radius = 50.f;
	float Rotation = 0.f;
};

/** World-space splat records plus a RenderTarget hook for the art-owned paint material. */
UCLASS()
class SLIMEWAR_API USlimePaintSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BeginRun();
	void ResetPaint();
	void ConfigureWorldBounds(const FVector& Centre, const FVector2D& Size);
	void RegisterSplat(
		const FVector& Location,
		const FLinearColor& Color,
		float Radius,
		float Rotation = 0.f,
		ESlimeFaction Faction = ESlimeFaction::A,
		float SourceCapsuleRadius = 20.f);

	UTextureRenderTarget2D* GetPaintRenderTarget() const { return PaintRenderTarget; }
	int32 GetSplatCount() const { return SplatCount; }
	const TArray<FSlimePaintSplat>& GetSplatRecords() const { return SplatRecords; }
	FLinearColor GetFactionColor(ESlimeFaction Faction) const;

private:
	void EnsureRenderTarget();
	void DrawSplatToRenderTarget(const FSlimePaintSplat& Splat);
	void SpawnPaintDecal(const FSlimePaintSplat& Splat, UMaterialInterface* DecalMaterial);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PaintRenderTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SplatMID = nullptr;

	TArray<TWeakObjectPtr<UDecalComponent>> PaintDecals;

	TArray<FSlimePaintSplat> SplatRecords;
	FVector WorldCentre = FVector::ZeroVector;
	FVector2D WorldSize = FVector2D(3000.f, 4400.f);
	int32 SplatCount = 0;
	bool bMaterialResolved = false;
};
