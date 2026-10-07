// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Character.h"
#include "SlimeEnemyBase.generated.h"

class UStaticMeshComponent;
class UStateTree;
class USlimeHealthComponent;
class USlimeFusionComponent;
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

	/** Fusion flow (PB-09~PB-14). Null on aggressive slimes: they never fuse. */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	USlimeFusionComponent* GetFusionComponent() const { return Fusion; }

	/**
	 * Look up the stat row for a mass tier and apply health / speed / size / mesh.
	 * HealthFraction is forwarded to USlimeHealthComponent::InitializeHealth so a fused slime
	 * keeps the pooled remaining-health ratio of its parents (PB-12).
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyStatRow(int32 NewMass, float HealthFraction = 1.f);

	/** Apply the DT_AggroStats row. Aggressive individuals have no mass tier. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyAggroStatRow();

	/**
	 * Set the capsule radius without letting the capsule sink into the floor.
	 * UCapsuleComponent::SetCapsuleRadius forces half height >= radius, and the actor origin is the
	 * capsule centre, so an oversized radius would push the bottom below the ground and wedge the
	 * character. See the implementation for the compensation.
	 */
	void ApplyBodyRadius(float Radius);

	/** Row name looked up in DT_AggroStats (falls back to "Default"). */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	FName GetAggroRowName() const;

	// -- Spawn / point identity --

	/**
	 * Called by the spawner (spawn points, cheats). Safe to call after BeginPlay.
	 *
	 * @param InActivityRadius cm; the radius this slime must stay inside around InActivityCenter.
	 *                         0 = unset, in which case the AI falls back to the global
	 *                         USlimeRunConfig::AIActivityRadius.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void InitializeFromSpawn(int32 InPointId, const FVector& InActivityCenter, float InActivityRadius = 0.f);

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	int32 GetPointId() const { return PointId; }

	/** Centre of the activity radius. Falls back to the spawn location when unset. */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	FVector GetActivityCenter() const { return ActivityCenter; }

	/** Radius this normal slime must stay inside. 0 = unset (the AI uses the global value). */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	float GetActivityRadius() const { return ActivityRadius; }

	/** True while inside GetActivityRadius() around GetActivityCenter() (always true when unset). */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	bool IsInsideActivityArea(float SlackCm = 0.f) const;

	/** Pull a world position back inside the activity radius (X/Y only, Z is left alone). */
	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	FVector ClampToActivityArea(const FVector& WorldLocation) const;

	/** StateTree asset this individual runs. Filled by ASlimeNormal / ASlimeAggro. */
	TSoftObjectPtr<UStateTree> GetStateTreeAsset() const { return StateTreeAsset; }

	/** Set by the Phase A target selection task; consumed by the Phase B fusion handshake. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void SetFusionTarget(ASlimeEnemyBase* InTarget);

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	ASlimeEnemyBase* GetFusionTarget() const { return FusionTarget; }

	/** Clears every enemy state tag (death / despawn). */
	void ClearAllStateTags();

	/**
	 * Movement level: never let the player's capsule shove this enemy around.
	 *
	 * The slime's own move sweeps are the source of the "player walks into a slime and it gets
	 * launched" behaviour: while the slime walks towards its fusion meeting point with the player
	 * capsule inside it, its sweep reports a start-penetrating blocking hit and the engine ejects
	 * it along the penetration normal. IgnoreActorWhenMoving removes the player from exactly this
	 * component's sweeps (see FPrimitiveComponent::InitSweepCollisionParams), so no hit -> no
	 * ejection. The channel responses are untouched, so aggro slimes are still blocked by normal
	 * slimes (PB-16).
	 *
	 * Safe to call repeatedly; it is refreshed whenever the player pawn may have changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void IgnorePlayerForMovement();

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

	/** Created by ASlimeNormal; aggressive slimes leave it null (see the class comment). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<USlimeFusionComponent> Fusion;

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

	/**
	 * Radius of the point this slime belongs to (design 5.1: normal targets only act inside their
	 * own point). Filled by InitializeFromSpawn from the spawn point's effective radius.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Enemy")
	float ActivityRadius = 0.f;

	/** Phase A: which state tree to run. Filled by the concrete subclasses. */
	UPROPERTY(EditDefaultsOnly, Category = "Slime|Enemy")
	TSoftObjectPtr<UStateTree> StateTreeAsset;

	/** Partner chosen by the target selection task. Cleared on death / target loss. */
	UPROPERTY(Transient)
	TObjectPtr<ASlimeEnemyBase> FusionTarget = nullptr;
};
