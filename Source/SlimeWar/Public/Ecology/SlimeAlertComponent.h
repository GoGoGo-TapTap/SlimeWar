// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeAlertComponent.generated.h"

class ASlimeEcologyEnemy;

/** Alert warning/duration, NPC-damage propagation and resource-dispute accumulation. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeAlertComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeAlertComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void RaiseAlert(AActor* Source, bool bPropagate);
	void NotifyNpcDamage(AActor* Causer);
	void SetDisputeCondition(bool bSatisfied, float DeltaTime);
	void ClearAlert();

	bool IsWarning() const { return WarningRemaining > 0.f; }
	bool IsAlerted() const { return WarningRemaining > 0.f || AlertRemaining > 0.f; }
	float GetWarningRemaining() const { return WarningRemaining; }
	float GetAlertRemaining() const { return AlertRemaining; }
	float GetDisputeAccumulator() const { return DisputeAccumulator; }
	float GetDisputeCooldownRemaining() const { return DisputeCooldownRemaining; }
	AActor* GetAlertSource() const { return AlertSource.Get(); }

private:
	ASlimeEcologyEnemy* GetOwnerEnemy() const;
	bool HasLineOfSight(const AActor* Target) const;
	bool CanReceiveAlert() const;

	TWeakObjectPtr<AActor> AlertSource;
	float WarningRemaining = 0.f;
	float AlertRemaining = 0.f;
	float DisputeAccumulator = 0.f;
	float DisputeCooldownRemaining = 0.f;
};
