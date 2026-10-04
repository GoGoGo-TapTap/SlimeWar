// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Character.h"
#include "SlimeEnemyBase.generated.h"

class UStaticMeshComponent;
class USlimeHealthComponent;
class USlimeStateComponent;

/**
 * Base enemy. Deliberately has NO AbilitySystemComponent (layered GAS decision): health is
 * authoritative on USlimeHealthComponent and state is expressed with USlimeStateComponent,
 * so 180 simultaneous enemies stay cheap.
 *
 * Not abstract in Phase 0 so it can be dropped into the sandbox map to verify damage.
 * It becomes abstract again once ASlimeNormal / ASlimeAggro exist (Phase A).
 * Slimes are static meshes; the inherited skeletal mesh is disabled.
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
	ETargetKind GetTargetKind() const { return TargetKind; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	USlimeHealthComponent* GetHealthComponent() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Slime|Enemy")
	USlimeStateComponent* GetStateComponent() const { return State; }

	/** Look up the stat row for a mass tier and apply health / speed / size / mesh. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Enemy")
	void ApplyStatRow(int32 NewMass);

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

	/** Mass / body size tier, 1..8. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slime|Enemy")
	int32 Mass = 1;
};
