// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SlimeWarCoreTypes.h"
#include "SlimeWeaponComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlimeAmmoChangedSignature, int32, CurrentAmmo, int32, MagazineSize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSlimeWeaponHitSignature, AActor*, HitActor, FVector, ImpactPoint, bool, bHit);

/**
 * Weapon shell. Phase 0 stores the stat row and owns the current magazine ammo.
 *
 * Ammo ownership is decided here and only here (plan Q11 / PA-14): the AttributeSet keeps the
 * configuration values (magazine size, reload duration, fire rate) while the ever changing
 * "rounds left" lives on this component. Never keep two copies.
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class USlimeWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeWeaponComponent();

	/** Cache the data table row and refill the magazine. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Weapon")
	void SetupFromStatRow(FName InWeaponId, const FWeaponStatRow& Row);

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	FName GetWeaponId() const { return WeaponId; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	int32 GetMagazineSize() const { return MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	FWeaponStatRow GetStatRow() const { return StatRow; }

	/** Remove one round. Returns false when the magazine is empty (nothing is removed). */
	UFUNCTION(BlueprintCallable, Category = "Slime|Weapon")
	bool ConsumeShot();

	/** Refill the magazine to its size (reload finished). Broadcasts OnAmmoChanged. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Weapon")
	void RefillMagazine();

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	bool IsMagazineEmpty() const { return MagazineAmmo <= 0; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	bool IsMagazineFull() const { return MagazineSize > 0 && MagazineAmmo >= MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	float GetDamage() const { return StatRow.Damage; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	float GetRange() const { return StatRow.Range; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	float GetFireRate() const { return StatRow.FireRate; }

	UFUNCTION(BlueprintPure, Category = "Slime|Weapon")
	float GetReloadDuration() const { return StatRow.ReloadDuration; }

	/** Presentation hook for Phase D / PE: reports where the shot went. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Weapon")
	void BroadcastShotResult(AActor* HitActor, FVector ImpactPoint, bool bHit);

	UPROPERTY(BlueprintAssignable, Category = "Slime|Weapon")
	FSlimeAmmoChangedSignature OnAmmoChanged;

	/** Fired for every shot attempt, hit or miss. No gameplay logic consumes it in Phase A. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Weapon")
	FSlimeWeaponHitSignature OnWeaponHit;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Weapon")
	FName WeaponId;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Weapon")
	FWeaponStatRow StatRow;

	/** Rounds currently in the magazine. The single source of truth for ammo. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Weapon")
	int32 MagazineAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Weapon")
	int32 MagazineSize = 0;
};
