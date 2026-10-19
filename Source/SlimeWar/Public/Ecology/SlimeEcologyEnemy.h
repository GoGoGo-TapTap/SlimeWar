// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Enemy/SlimeEnemyBase.h"
#include "SlimeEcologyEnemy.generated.h"

class USlimeEcologyBrainComponent;
class USlimeAlertComponent;
class USlimeEcologyFusionComponent;
class USlimeBodyInteractionComponent;
class USlimeCoreComponent;
class ASlimeFactionSite;
class ASlimeCargoVisual;
class USlimeLocomotionComponent;
class USlimeWeakpointComponent;

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
	USlimeAlertComponent* GetAlertComponent() const { return Alert; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeLocomotionComponent* GetLocomotion() const { return Locomotion; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeBodyInteractionComponent* GetBodyInteraction() const { return BodyInteraction; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeEcologyFusionComponent* GetEcologyFusionComponent() const { return EcologyFusion; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeCoreComponent* GetCoreComponent() const { return Core; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	USlimeWeakpointComponent* GetWeakpointComponent() const { return Weakpoint; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	int32 GetCargo() const { return Cargo; }

	UFUNCTION(BlueprintPure, Category = "Slime|Ecology")
	int32 GetBodyScoreCredit() const { return BodyScoreCredit; }

	UFUNCTION(BlueprintCallable, Category = "Slime|Ecology")
	void SetEcologyTotals(int32 InCargo, int32 InBodyScoreCredit);

	UFUNCTION(BlueprintCallable, Category = "Slime|Ecology")
	void SetCargo(int32 InCargo);

	UFUNCTION(BlueprintCallable, Category = "Slime|Ecology")
	void AddCargo(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Slime|Ecology")
	bool ConsumeCargo(int32 Count = 1);

	void SetAssignedSite(ASlimeFactionSite* InSite);
	ASlimeFactionSite* GetAssignedSite() const { return AssignedSite.Get(); }

	/** Spawn/update the attachable cargo model; the class is replaceable like the slime mesh. */
	void RefreshCarriedCargoVisual();
	void ApplyAssimilation(ESlimeFaction InFaction);
	void MarkChainKill() { bChainKill = true; }
	void NotifyCombatDamage(AActor* Causer);

	static int32 BodyScoreForMass(int32 InMass);

	/** Charge-impact split. Commits only when every fragment position is legal. */
	bool TryImpactSplit(const FVector& ImpactPoint, const FVector& ImpactNormal);

	/** Ecology bodies collide with the player; they never use the legacy movement ignore. */
	virtual void IgnorePlayerForMovement() override;

protected:
	virtual void HandleDeath() override;
	virtual void OnBodyStatsApplied(int32 NewMass, float BodyRadius) override;
	virtual FSlimeKillContext BuildKillContext() const override;

	UFUNCTION()
	void HandleDamaged(AActor* Causer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeEcologyBrainComponent> Brain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeAlertComponent> Alert;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeLocomotionComponent> Locomotion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeBodyInteractionComponent> BodyInteraction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeEcologyFusionComponent> EcologyFusion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeCoreComponent> Core;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	TObjectPtr<USlimeWeakpointComponent> Weakpoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Ecology")
	ESlimeFaction Faction = ESlimeFaction::A;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Ecology")
	int32 Cargo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Ecology")
	int32 BodyScoreCredit = 0;

	TWeakObjectPtr<ASlimeFactionSite> AssignedSite;
	TWeakObjectPtr<ASlimeCargoVisual> CarriedCargoVisual;

	bool bSplitting = false;
	bool bChainKill = false;
	mutable TOptional<FSlimeKillContext> PendingKillContext;
};
