// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeMineralPatch.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

/** Mineable resource patch with optional regeneration. */
UCLASS()
class SLIMEWAR_API ASlimeMineralPatch : public AActor
{
	GENERATED_BODY()

public:
	ASlimeMineralPatch();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	FName PatchId = TEXT("Patch");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	int32 InitialStock = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	int32 MaxStock = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	bool bRegenerates = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	float RegrowSeconds = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mineral")
	float GatherRange = 120.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mineral")
	int32 Stock = 0;

	int32 ConsumeOne();
	void NotifyRegenerated() { ++RegeneratedCount; }
	int32 GetRegeneratedCount() const { return RegeneratedCount; }
	float GetRegrowRemaining() const { return RegrowAccumulator; }

protected:
	void RefreshLabel();

	UPROPERTY(VisibleAnywhere, Category = "Mineral")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Mineral")
	TObjectPtr<UStaticMeshComponent> CrystalMesh;

	UPROPERTY(VisibleAnywhere, Category = "Mineral")
	TObjectPtr<UTextRenderComponent> Label;

	float RegrowAccumulator = 0.f;
	int32 RegeneratedCount = 0;
};
