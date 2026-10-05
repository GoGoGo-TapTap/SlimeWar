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

bool USlimeWeaponComponent::ConsumeShot()
{
	if (MagazineAmmo <= 0)
	{
		return false;
	}

	--MagazineAmmo;
	OnAmmoChanged.Broadcast(MagazineAmmo, MagazineSize);
	return true;
}

void USlimeWeaponComponent::RefillMagazine()
{
	MagazineAmmo = MagazineSize;
	OnAmmoChanged.Broadcast(MagazineAmmo, MagazineSize);
}

void USlimeWeaponComponent::BroadcastShotResult(AActor* HitActor, FVector ImpactPoint, bool bHit)
{
	OnWeaponHit.Broadcast(HitActor, ImpactPoint, bHit);
}
