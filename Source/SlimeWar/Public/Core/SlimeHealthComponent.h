// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlimeHealthChangedSignature, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlimeDeathSignature);

/**
 * Health for every damageable actor. Deliberately free of any GAS dependency so that
 * enemies keep a cheap authoritative health and the player can mirror its AttributeSet.
 *
 * Authoritative mode (enemies): ApplyDamage drives Health and broadcasts OnDeath.
 * Proxy mode (player):        SetHealthFromProxy mirrors the player AttributeSet; ApplyDamage
 *                             refuses to run so there is never a second source of truth.
 *
 * Either way the outside world only sees OnHealthChanged / OnDeath.
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class USlimeHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeHealthComponent();

	/**
	 * Set max health and start at MaxHealth * HealthFraction.
	 * Numbers come from DataTable / DataAsset, never from code.
	 *
	 * HealthFraction exists for PB-12: a fused slime inherits the pooled remaining-health ratio
	 * of its parents instead of healing to full. The default (1.0) keeps the Phase C pool reset
	 * and every aggro spawn at full health.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Health")
	void InitializeHealth(float InMaxHealth, float HealthFraction = 1.f);

	/** Authoritative damage entry. No-op in proxy mode. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Health")
	void ApplyDamage(float Amount, AActor* Causer);

	/** Restore to full and clear the dead flag (pooling / re-init path). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Health")
	void RestoreToFull();

	/** Proxy mode only: mirror the values owned by the player AttributeSet. Never writes back. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Health")
	void SetHealthFromProxy(float NewHealth, float NewMaxHealth);

	UFUNCTION(BlueprintPure, Category = "Slime|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Slime|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Slime|Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Slime|Health")
	bool IsAuthoritative() const { return bAuthoritative; }

	UPROPERTY(BlueprintAssignable, Category = "Slime|Health")
	FSlimeHealthChangedSignature OnHealthChanged;

	/** The single death exit point for both damage paths. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Health")
	FSlimeDeathSignature OnDeath;

	/** True for enemies (this component owns the health). False for the player (AttributeSet owns it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slime|Health")
	bool bAuthoritative = true;

protected:
	/** Max health. 0 = not configured yet, filled from data at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slime|Health")
	float MaxHealth = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Health")
	float Health = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Health")
	bool bDead = false;

private:
	void BroadcastDeathIfNeeded();
};
