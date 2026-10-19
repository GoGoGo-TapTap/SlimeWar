// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeBodyInteractionComponent.generated.h"

class USlimeEcologyRules;

UENUM(BlueprintType)
enum class ESlimeBodyState : uint8
{
	Normal,
	AimLocked,
	StunPending,
	Stunned
};

/** External motion, body pushes, aim lock and post-stop stun for one ecology slime. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeBodyInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeBodyInteractionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void ReceiveBodyPush(const FVector& Direction, float Speed, AActor* Source);
	void ApplyExternalVelocity(const FVector& DeltaVelocity, AActor* Source = nullptr);
	void SetExternalVelocity(const FVector& InVelocity, AActor* Source = nullptr);
	void OpenImpactSplitWindow(float Seconds = -1.f);
	float GetImpactSplitWindowRemaining() const { return ImpactSplitWindowRemaining; }

	void SetAimLocked(bool bLocked, AActor* Source = nullptr);
	bool IsAimLocked() const { return AimLockSource.IsValid(); }

	void ReceiveStun(float Duration);
	bool IsStunPending() const { return StunPending > 0.f; }
	bool IsStunned() const { return StunRemaining > 0.f; }
	bool BlocksVoluntaryMovement() const
	{
		return IsAimLocked() || StunPending > 0.f || StunRemaining > 0.f;
	}

	ESlimeBodyState GetBodyState() const;
	const FVector& GetExternalVelocity() const { return ExternalVelocity; }
	float GetObservedSpeed() const { return ObservedSpeed; }
	float GetStunPending() const { return StunPending; }
	float GetStunRemaining() const { return StunRemaining; }
	float GetSettleTime() const { return SettleTime; }
	float GetForcedFusionWindowRemaining() const { return ForcedFusionWindowRemaining; }
	bool IsSettled() const { return StunPending > 0.f && SettleTime > 0.f; }
	void ClearForcedFusionWindow() { ForcedFusionWindowRemaining = 0.f; }

private:
	const USlimeEcologyRules* GetRules() const;
	void ApplyExternalMotion(float DeltaTime);
	void UpdateStun(float DeltaTime);

	FVector ExternalVelocity = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> AimLockSource;
	TWeakObjectPtr<AActor> LastPushSource;
	float ObservedSpeed = 0.f;
	float StunPending = 0.f;
	float StunRemaining = 0.f;
	float SettleTime = 0.f;
	float ForcedFusionWindowRemaining = 0.f;
	float ImpactSplitWindowRemaining = 0.f;
	float LastBodyPushTime = -100.f;
};
