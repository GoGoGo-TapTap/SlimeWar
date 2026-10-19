// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeCargoVisual.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** Replaceable cargo visual shared by carried cargo and ground pickups. */
UCLASS()
class SLIMEWAR_API ASlimeCargoVisual : public AActor
{
	GENERATED_BODY()

public:
	ASlimeCargoVisual();

	virtual void BeginPlay() override;

	void SetVisualAmount(int32 InAmount);
	void SetVisualMesh(UStaticMesh* InMesh);

	UFUNCTION(BlueprintPure, Category = "Slime|Cargo")
	UStaticMeshComponent* GetCargoMeshComponent() const { return CargoMesh; }

protected:
	void RefreshVisual();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo|Visual")
	TSoftObjectPtr<UStaticMesh> CargoMeshAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cargo|Visual")
	float BaseScale = 0.24f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cargo|Visual")
	TObjectPtr<UStaticMeshComponent> CargoMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Cargo|Visual")
	int32 VisualAmount = 1;
};
