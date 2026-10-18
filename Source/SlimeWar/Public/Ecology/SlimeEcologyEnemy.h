// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Enemy/SlimeEnemyBase.h"
#include "SlimeEcologyEnemy.generated.h"

class USlimeEcologyBrainComponent;
class USlimeBodyInteractionComponent;
class USlimeLocomotionComponent;

/** First runtime ecology body. It deliberately does not use the legacy StateTree assets. */
UCLASS()
class SLIMEWAR_API ASlimeEcologyEnemy : public ASlimeEnemyBase
{
	GENERATED_BODY()

public:
	ASlimeEcologyEnemy();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeEcology(
		ESlimeFaction InFaction,
		int32 InPointId,
		const FVector& InActivityCenter,
		float InActivityRadius);

	UFUNCTION(BlueprintCallable, Category = "Slime|Ecology")
	void SetFaction(ESlimeFaction InFaction);

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	ESlimeFaction GetFaction() const { return Faction; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeEcologyBrainComponent* GetEcologyBrain() const { return Brain; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeLocomotionComponent* GetLocomotion() const { return Locomotion; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeBodyInteractionComponent* GetBodyInteraction() const { return BodyInteraction; }

	/** Ecology bodies collide with the player; they never use the legacy movement ignore. */
	virtual void IgnorePlayerForMovement() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeEcologyBrainComponent> Brain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeLocomotionComponent> Locomotion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeBodyInteractionComponent> BodyInteraction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	ESlimeFaction Faction = ESlimeFaction::A;
};
