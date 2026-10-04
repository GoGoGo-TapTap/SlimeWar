// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWeaponComponent.h"

USlimeWeaponComponent::USlimeWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeWeaponComponent::SetupFromStatRow(FName InWeaponId, const FWeaponStatRow& Row)
{
	WeaponId = InWeaponId;
	StatRow = Row;
	MagazineSize = Row.MagazineSize;
	MagazineAmmo = MagazineSize;

	OnAmmoChanged.Broadcast(MagazineAmmo, MagazineSize);
}
