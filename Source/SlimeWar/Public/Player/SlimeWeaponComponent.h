// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SlimeWarCoreTypes.h"
#include "SlimeWeaponComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlimeAmmoChangedSignature, int32, CurrentAmmo, int32, MagazineSize);

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

	UPROPERTY(BlueprintAssignable, Category = "Slime|Weapon")
	FSlimeAmmoChangedSignature OnAmmoChanged;

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
