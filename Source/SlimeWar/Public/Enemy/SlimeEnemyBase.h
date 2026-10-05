// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Character.h"
#include "SlimeEnemyBase.generated.h"

class UStaticMeshComponent;
class UStateTree;
class USlimeHealthComponent;
class USlimeStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimeEnemyDiedSignature, AActor*, Enemy);

/**
 * Base enemy. Deliberately has NO AbilitySystemComponent (layered GAS decision): health is
 * authoritative on USlimeHealthComponent and state is expressed with USlimeStateComponent,
 * so 180 simultaneous enemies stay cheap.
 *
 * Phase A made this class abstract: spawn ASlimeNormal or ASlimeAggro instead.
 * Slimes are static meshes; the inherited skeletal mesh is disabled.
 *
 * AI lives in StateTree, run by ASlimeAIController:
 *   ASlimeNormal -> ST_SlimeNormal, ASlimeAggro -> ST_SlimeAggro.
 */
UCLASS(Abstract)
class ASlimeEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	ASlimeEnemyBase();

	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	int32 GetMass() const { return Mass; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	ETargetKind GetTargetKind() const { return TargetKind; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	bool IsAggressive() const { return TargetKind == ETargetKind::Aggressive; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	USlimeHealthComponent* GetHealthComponent() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	USlimeStateComponent* GetStateComponent() const { return State; }

	/** Look up the stat row for a mass tier and apply health / speed / size / mesh. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyStatRow(int32 NewMass);

	/** Apply the DT_AggroStats row. Aggressive individuals have no mass tier. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyAggroStatRow();

	/** Row name looked up in DT_AggroStats (falls back to "Default"). */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	FName GetAggroRowName() const;

	// -- Spawn / point identity --

	/** Called by the spawner (spawn points, cheats). Safe to call after BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void InitializeFromSpawn(int32 InPointId, const FVector& InActivityCenter);

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	int32 GetPointId() const { return PointId; }

	/** Centre of the activity radius. Falls back to the spawn location when unset. */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	FVector GetActivityCenter() const { return ActivityCenter; }

	/** StateTree asset this individual runs. Filled by ASlimeNormal / ASlimeAggro. */
	TSoftObjectPtr<UStateTree> GetStateTreeAsset() const { return StateTreeAsset; }

	/** Set by the Phase A target selection task; consumed by the Phase B fusion handshake. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void SetFusionTarget(ASlimeEnemyBase* InTarget);

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	ASlimeEnemyBase* GetFusionTarget() const { return FusionTarget; }

	/** Clears every enemy state tag (death / despawn). */
	void ClearAllStateTags();

	UPROPERTY(BlueprintAssignable, Category = "Slime|Enemy")
	FSlimeEnemyDiedSignature OnEnemyDied;

protected:
	UFUNCTION()
	virtual void HandleDeath();

	/** Death: notify the battle director. Only normal targets award score. */
	void NotifyDirectorOnDeath();

	/** Authoritative health. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<USlimeHealthComponent> Health;

	/** Lightweight tag state (not GAS). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<USlimeStateComponent> State;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	ETargetKind TargetKind = ETargetKind::Normal;

	/** Mass / body size tier, 1..8. Unused by aggressive individuals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	int32 Mass = 1;

	/** Spawn point this individual belongs to. Normal slimes only fuse inside their own point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	int32 PointId = 0;

	/** Centre of the activity radius, world space. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Enemy")
	FVector ActivityCenter = FVector::ZeroVector;

	/** Phase A: which state tree to run. Filled by the concrete subclasses. */
	UPROPERTY(EditDefaultsOnly, Category = "Slime|Enemy")
	TSoftObjectPtr<UStateTree> StateTreeAsset;

	/** Partner chosen by the target selection task. Cleared on death / target loss. */
	UPROPERTY(Transient)
	TObjectPtr<ASlimeEnemyBase> FusionTarget = nullptr;
};
