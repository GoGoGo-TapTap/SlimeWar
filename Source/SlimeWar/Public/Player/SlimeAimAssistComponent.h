// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeAimAssistComponent.generated.h"

class AActor;
class ASlimeEcologyEnemy;
class ASlimeWarCharacter;
class UPrimitiveComponent;
class USlimeWeaponRules;

/** Result of camera/muzzle aim solving for one shot. */
struct FSlimeShotSolution
{
	FVector AimPoint = FVector::ZeroVector;
	FVector MuzzleEnd = FVector::ZeroVector;
	FHitResult HitResult;
	AActor* TargetActor = nullptr;
	UPrimitiveComponent* HitComponent = nullptr;
	bool bWeakpoint = false;
	bool bHit = false;
};

/** Screen-space magnetic aim with body/weakpoint candidates and camera + muzzle occlusion. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeAimAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeAimAssistComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	AActor* GetLockedTarget() const;
	UPrimitiveComponent* GetLockedComponent() const { return LockedComponent.Get(); }
	bool IsWeakpointLocked() const { return bLockedWeakpoint; }
	FVector GetLockedAimPoint() const;
	float GetLastCaptureRadius() const { return LastCaptureRadius; }
	float GetLastScreenDistance() const { return LastScreenDistance; }

	bool SolveShot(FSlimeShotSolution& OutSolution) const;
	void ReleaseLock();

	/** Debug/test hook: bypass screen scan but still route through the normal aim-lock state. */
	bool DebugForceLock(ASlimeEcologyEnemy* Target, bool bWeakpoint);
	bool DebugEvaluateTarget(
		ASlimeEcologyEnemy* Target,
		bool bWeakpoint,
		float& OutScreenDistance) const;

private:
	struct FCandidate
	{
		TWeakObjectPtr<ASlimeEcologyEnemy> Target;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		bool bWeakpoint = false;
		FVector WorldPoint = FVector::ZeroVector;
		float ScreenDistance = TNumericLimits<float>::Max();
	};

	ASlimeWarCharacter* GetOwnerCharacter() const;
	const USlimeWeaponRules* GetRules() const;
	FVector GetMuzzleLocation() const;

	bool EvaluateCandidate(
		ASlimeEcologyEnemy* Target,
		bool bWeakpoint,
		const FVector2D& ScreenCentre,
		float CaptureRadius,
		FCandidate& OutCandidate) const;
	bool IsVisibleFromCameraAndMuzzle(
		const FVector& WorldPoint,
		const AActor* Target) const;
	void ScanForTarget();
	void SetLockedCandidate(const FCandidate& Candidate);
	void ClearLock();

	TWeakObjectPtr<ASlimeEcologyEnemy> LockedTarget;
	TWeakObjectPtr<UPrimitiveComponent> LockedComponent;
	bool bLockedWeakpoint = false;
	float ScanCountdown = 0.f;
	float LastCaptureRadius = 0.f;
	float LastScreenDistance = -1.f;
};
