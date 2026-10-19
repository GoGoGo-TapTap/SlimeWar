// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Character.h"
#include "SlimeEnemyBase.generated.h"

class UStaticMeshComponent;
class USlimeHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimeEnemyDiedSignature, AActor*, Enemy);

/**
 * Base ecology body. Deliberately has NO AbilitySystemComponent (layered GAS decision): health is
 * authoritative on USlimeHealthComponent, so many simultaneous bodies stay cheap.
 *
 * Slimes are static meshes; the inherited skeletal mesh is disabled.
 *
 * This class owns only shared body concerns (health, mass, grounding, mesh fit). Ecology AI and
 * interaction live on ASlimeEcologyEnemy components; the StateTree/Normal/Aggro routes are gone.
 */
UCLASS()
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
	USlimeHealthComponent* GetHealthComponent() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	UStaticMeshComponent* GetBodyMeshComponent() const { return BodyMesh; }

	/**
	 * Look up the stat row for a mass tier and apply health / speed / size / mesh.
	 * HealthFraction is forwarded to USlimeHealthComponent::InitializeHealth so a fused slime
	 * keeps the pooled remaining-health ratio of its parents (PB-12).
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyStatRow(int32 NewMass, float HealthFraction = 1.f);

	/**
	 * Resize the body to a sphere whose bottom stays at the same world Z.
	 *
	 * The engine capsule origin is its centre, while the Godot slime origin is at its feet. The
	 * capsule is therefore kept spherical (half height == radius) and the actor is moved after a
	 * resize so its feet do not move. The visual mesh is then fitted to that same sphere.
	 */
	void ApplyBodyRadius(float Radius);

	// -- Spawn / point identity --

	/**
	 * Called by the spawner (spawn points, cheats). Safe to call after BeginPlay.
	 *
	 * @param InActivityRadius cm; the radius this slime must stay inside around InActivityCenter.
	 *                         0 = unset, in which case the AI falls back to the ecology rules'
	 *                         wander radius.
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

	/**
	 * Phase D: freeze everything this actor drives, without touching the run flow.
	 *
	 * The result camera needs the enemies to stand still for a couple of seconds (design 2.6), and
	 * Flow is the one that knows the run just ended - so Flow pushes this down (plan decision D4).
	 * A global time dilation or an engine pause would also stop the Level Sequence camera and the
	 * UI, which is why this is an explicit, per-actor freeze.
	 *
	 * Freezing also drops any fusion pairing: a pair that was mid-approach must not survive into
	 * the settlement screen with a stale partner reference.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void SetRunFrozen(bool bFrozen);

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	bool IsRunFrozen() const { return bRunFrozen; }

	UPROPERTY(BlueprintAssignable, Category = "Slime|Enemy")
	FSlimeEnemyDiedSignature OnEnemyDied;

protected:
	UFUNCTION()
	virtual void HandleDeath();

	/** Death: notify the battle director. Only normal targets award score. */
	void NotifyDirectorOnDeath();

	virtual FSlimeKillContext BuildKillContext() const;

	/** Authoritative health. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Used when a stat row has no mesh of its own (the current placeholder rows). */
	UPROPERTY(EditDefaultsOnly, Category = "Slime|Enemy")
	TSoftObjectPtr<UStaticMesh> DefaultBodyMesh;

	/** Fit and centre the static mesh so its visual bottom matches the capsule bottom. */
	void FitBodyMeshToBodyRadius(float Radius);

	/** Derived ecology bodies refresh weakpoints/cores after every mass/size change. */
	virtual void OnBodyStatsApplied(int32 NewMass, float BodyRadius);

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

	/** True while the result camera / a frozen world owns this actor (Phase D). */
	bool bRunFrozen = false;
};
