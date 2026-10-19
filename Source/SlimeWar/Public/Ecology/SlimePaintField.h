// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimePaintField.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class USlimePaintSubsystem;

/** Single ground plane that samples the paint RenderTarget. Material is supplied by art later. */
UCLASS()
class SLIMEWAR_API ASlimePaintField : public AActor
{
	GENERATED_BODY()

public:
	ASlimePaintField();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FVector2D WorldSize = FVector2D(3000.f, 4400.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UMaterialInterface> PaintMaterial = nullptr;

protected:
	USlimePaintSubsystem* GetPaintSubsystem() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint")
	TObjectPtr<UStaticMeshComponent> Surface;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID = nullptr;
};
