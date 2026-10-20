// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Actor.h"
#include "SlimeFactionSite.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Runtime faction delivery/spawn centre. */
UCLASS()
class SLIMEWAR_API ASlimeFactionSite : public AActor
{
	GENERATED_BODY()

public:
	ASlimeFactionSite();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	FName SiteId = TEXT("A");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	ESlimeFaction Faction = ESlimeFaction::A;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	int32 InitialPopulation = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	float ActivityRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	float DeliveryRadius = 130.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	float SpawnRingMin = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	float SpawnRingMax = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Site")
	FLinearColor DisplayColor = FLinearColor(0.518f, 0.863f, 0.996f, 1.f);

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	FVector GetDeliveryLocation() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	int32 GetDeliveredCount() const { return DeliveredCount; }

	void NotifyDelivered() { ++DeliveredCount; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Site")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Site")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Site")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(VisibleInstanceOnly, Category = "Site")
	int32 DeliveredCount = 0;
};
