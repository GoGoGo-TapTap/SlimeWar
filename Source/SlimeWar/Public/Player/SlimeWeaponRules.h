// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeWeaponRules.generated.h"

/** M6 rifle, charge and magnetic-aim tuning. */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeWeaponRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const USlimeWeaponRules* GetFor(const UObject* WorldContextObject);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float BaseDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireInterval = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float ReloadDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Range = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float ChargeStart = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float ChargeFullTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float ChargePushRadius = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float ChargePushSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float DirectPushSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float DirectPushMassExponent = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge")
	float RadialPushEdgeScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic Aim")
	float AimCaptureRatio = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic Aim")
	float AimRetentionRatio = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic Aim")
	float AimSwitchRatio = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic Aim")
	float AimScanInterval = 0.05f;
};
