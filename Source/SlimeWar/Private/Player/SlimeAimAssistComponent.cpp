// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeAimAssistComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeWeakpointComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Player/SlimeWarCharacter.h"
#include "Player/SlimeWeaponComponent.h"
#include "Player/SlimeWeaponRules.h"

USlimeAimAssistComponent::USlimeAimAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

AActor* USlimeAimAssistComponent::GetLockedTarget() const
{
	return LockedTarget.Get();
}

ASlimeWarCharacter* USlimeAimAssistComponent::GetOwnerCharacter() const
{
	return Cast<ASlimeWarCharacter>(GetOwner());
}

const USlimeWeaponRules* USlimeAimAssistComponent::GetRules() const
{
	return USlimeWeaponRules::GetFor(this);
}

void USlimeAimAssistComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const USlimeWeaponRules* Rules = GetRules();
	if (!Rules)
	{
		return;
	}

	ScanCountdown -= DeltaTime;
	if (ScanCountdown > 0.f)
	{
		return;
	}
	ScanCountdown = FMath::Max(0.01f, Rules->AimScanInterval);
	ScanForTarget();
}

void USlimeAimAssistComponent::ScanForTarget()
{
	ASlimeWarCharacter* Character = GetOwnerCharacter();
	const APlayerController* PlayerController =
		Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	const USlimeWeaponRules* Rules = GetRules();
	const USlimeWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	const UWorld* World = GetWorld();
	if (!Character || !PlayerController || !Rules || !Weapon || !World
		|| Character->IsRunInputBlocked()
		|| (Character->GetHealthComponent() && Character->GetHealthComponent()->IsDead()))
	{
		ClearLock();
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		ClearLock();
		return;
	}

	const FVector2D ScreenCentre(ViewportX * 0.5f, ViewportY * 0.5f);
	const float CaptureRadius = ViewportY * Rules->AimCaptureRatio;
	LastCaptureRadius = CaptureRadius;

	FCandidate Current;
	bool bCurrentValid = false;
	if (ASlimeEcologyEnemy* CurrentTarget = LockedTarget.Get())
	{
		if (EvaluateCandidate(
			CurrentTarget,
			bLockedWeakpoint,
			ScreenCentre,
			CaptureRadius * Rules->AimRetentionRatio,
			Current))
		{
			bCurrentValid = true;
			LastScreenDistance = Current.ScreenDistance;
		}
	}

	const USlimeEcologySubsystem* Ecology =
		World->GetSubsystem<USlimeEcologySubsystem>();
	if (!Ecology)
	{
		ClearLock();
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryAgentsAroundLocation(
		Character->GetActorLocation(),
		Weapon->GetRange(),
		Neighbours);

	FCandidate Best = bCurrentValid ? Current : FCandidate();
	bool bHasBest = bCurrentValid;
	for (ASlimeEcologyEnemy* CandidateTarget : Neighbours)
	{
		if (!CandidateTarget || CandidateTarget == LockedTarget.Get())
		{
			continue;
		}

		for (const bool bWeakpoint : {false, true})
		{
			FCandidate Candidate;
			if (!EvaluateCandidate(
				CandidateTarget,
				bWeakpoint,
				ScreenCentre,
				CaptureRadius,
				Candidate))
			{
				continue;
			}

			const bool bBetterDifferentTarget =
				!bHasBest
				|| (Best.Target.Get() != CandidateTarget
					&& Candidate.ScreenDistance < Best.ScreenDistance * Rules->AimSwitchRatio)
				|| (Best.Target.Get() == CandidateTarget
					&& Candidate.ScreenDistance < Best.ScreenDistance);
			if (bBetterDifferentTarget)
			{
				Best = Candidate;
				bHasBest = true;
			}
		}
	}

	if (!bHasBest)
	{
		ClearLock();
		return;
	}

	SetLockedCandidate(Best);
	LastScreenDistance = Best.ScreenDistance;
}

bool USlimeAimAssistComponent::EvaluateCandidate(
	ASlimeEcologyEnemy* Target,
	bool bWeakpoint,
	const FVector2D& ScreenCentre,
	float CaptureRadius,
	FCandidate& OutCandidate) const
{
	const ASlimeWarCharacter* Character = GetOwnerCharacter();
	const APlayerController* PlayerController =
		Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	const USlimeHealthComponent* Health =
		Target ? Target->GetHealthComponent() : nullptr;
	const USlimeWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!Target || !PlayerController || !Weapon || (Health && Health->IsDead()))
	{
		return false;
	}

	if (FVector::Dist2D(Character->GetActorLocation(), Target->GetActorLocation())
		> Weapon->GetRange())
	{
		return false;
	}

	UPrimitiveComponent* Component = Target->GetCapsuleComponent();
	FVector WorldPoint = Target->GetActorLocation();
	USlimeWeakpointComponent* Weakpoint = Target->GetWeakpointComponent();
	if (bWeakpoint)
	{
		if (!Weakpoint || !Weakpoint->IsWeakpointActive())
		{
			return false;
		}
		Component = Weakpoint;
		WorldPoint = Weakpoint->GetComponentLocation();
	}

	FVector2D Screen;
	if (!PlayerController->ProjectWorldLocationToScreen(WorldPoint, Screen, true))
	{
		return false;
	}

	const float ScreenDistance = FVector2D::Distance(Screen, ScreenCentre);
	if (ScreenDistance > CaptureRadius)
	{
		return false;
	}

	if (!IsVisibleFromCameraAndMuzzle(WorldPoint, Target))
	{
		return false;
	}

	OutCandidate.Target = Target;
	OutCandidate.Component = Component;
	OutCandidate.bWeakpoint = bWeakpoint;
	OutCandidate.WorldPoint = WorldPoint;
	OutCandidate.ScreenDistance = ScreenDistance;
	return true;
}

bool USlimeAimAssistComponent::IsVisibleFromCameraAndMuzzle(
	const FVector& WorldPoint,
	const AActor* Target) const
{
	const ASlimeWarCharacter* Character = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!Character || !Target || !World)
	{
		return false;
	}

	const UCameraComponent* Camera = Character->GetFollowCamera();
	if (!Camera)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAimVisibility), false, Character);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(
		Hit,
		Camera->GetComponentLocation(),
		WorldPoint,
		ECC_Visibility,
		Params))
	{
		return false;
	}

	if (World->LineTraceSingleByChannel(
		Hit,
		GetMuzzleLocation(),
		WorldPoint,
		ECC_Visibility,
		Params))
	{
		return false;
	}

	return true;
}

FVector USlimeAimAssistComponent::GetMuzzleLocation() const
{
	const ASlimeWarCharacter* Character = GetOwnerCharacter();
	const UCameraComponent* Camera = Character ? Character->GetFollowCamera() : nullptr;
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStatTableProvider* Provider =
		GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;
	if (!Camera || !Character)
	{
		return FVector::ZeroVector;
	}

	const FVector Offset = RunConfig ? RunConfig->MuzzleOffsetLocal : FVector::ZeroVector;
	// The offset is expressed in camera yaw/pitch, but the trace origin is the player body, not
	// the camera. A third-person camera may see over a wall while the character is fully behind it.
	return Character->GetActorLocation()
		+ Camera->GetComponentRotation().RotateVector(Offset);
}

void USlimeAimAssistComponent::SetLockedCandidate(const FCandidate& Candidate)
{
	ASlimeEcologyEnemy* NewTarget = Candidate.Target.Get();
	if (!NewTarget)
	{
		ClearLock();
		return;
	}

	ASlimeEcologyEnemy* OldTarget = LockedTarget.Get();
	if (OldTarget && OldTarget != NewTarget)
	{
		if (USlimeBodyInteractionComponent* OldBody = OldTarget->GetBodyInteraction())
		{
			OldBody->SetAimLocked(false, GetOwner());
		}
	}

	LockedTarget = NewTarget;
	LockedComponent = Candidate.Component;
	bLockedWeakpoint = Candidate.bWeakpoint;
	if (USlimeBodyInteractionComponent* Body = NewTarget->GetBodyInteraction())
	{
		Body->SetAimLocked(true, GetOwner());
	}
}

void USlimeAimAssistComponent::ClearLock()
{
	if (ASlimeEcologyEnemy* OldTarget = LockedTarget.Get())
	{
		if (USlimeBodyInteractionComponent* Body = OldTarget->GetBodyInteraction())
		{
			Body->SetAimLocked(false, GetOwner());
		}
	}
	LockedTarget.Reset();
	LockedComponent.Reset();
	bLockedWeakpoint = false;
	LastScreenDistance = -1.f;
}

void USlimeAimAssistComponent::ReleaseLock()
{
	ClearLock();
}

FVector USlimeAimAssistComponent::GetLockedAimPoint() const
{
	if (const UPrimitiveComponent* Component = LockedComponent.Get())
	{
		return Component->GetComponentLocation();
	}
	if (const ASlimeEcologyEnemy* Target = LockedTarget.Get())
	{
		return Target->GetActorLocation();
	}
	return FVector::ZeroVector;
}

bool USlimeAimAssistComponent::SolveShot(FSlimeShotSolution& OutSolution) const
{
	OutSolution = FSlimeShotSolution();

	const ASlimeWarCharacter* Character = GetOwnerCharacter();
	const UCameraComponent* Camera = Character ? Character->GetFollowCamera() : nullptr;
	const USlimeWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	const UWorld* World = GetWorld();
	if (!Character || !Camera || !Weapon || !World)
	{
		return false;
	}

	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector CameraForward = Camera->GetForwardVector();
	const float Range = Weapon->GetRange();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAimShot), false, Character);

	FVector AimPoint = CameraLocation + CameraForward * Range;
	AActor* CameraActor = nullptr;
	UPrimitiveComponent* CameraComponent = nullptr;
	if (const ASlimeEcologyEnemy* Locked = LockedTarget.Get())
	{
		AimPoint = GetLockedAimPoint();
		CameraActor = const_cast<ASlimeEcologyEnemy*>(Locked);
		CameraComponent = LockedComponent.Get();
	}
	else
	{
		FHitResult CameraHit;
		if (World->LineTraceSingleByChannel(
			CameraHit,
			CameraLocation,
			AimPoint,
			ECC_Visibility,
			Params))
		{
			AimPoint = CameraHit.ImpactPoint;
			CameraActor = CameraHit.GetActor();
			CameraComponent = CameraHit.GetComponent();
		}
	}

	const FVector Muzzle = GetMuzzleLocation();
	FCollisionQueryParams MuzzleParams(SCENE_QUERY_STAT(SlimeMuzzleShot), false, Character);
	FHitResult MuzzleHit;
	const bool bMuzzleHit = World->LineTraceSingleByChannel(
		MuzzleHit,
		Muzzle,
		AimPoint,
		ECC_Visibility,
		MuzzleParams);

	OutSolution.AimPoint = AimPoint;
	OutSolution.MuzzleEnd = bMuzzleHit ? MuzzleHit.ImpactPoint : AimPoint;

	AActor* FinalActor = CameraActor;
	UPrimitiveComponent* FinalComponent = CameraComponent;
	if (bMuzzleHit)
	{
		const bool bSameLockedTarget =
			LockedTarget.IsValid() && MuzzleHit.GetActor() == LockedTarget.Get();
		if (LockedTarget.IsValid() && !bSameLockedTarget)
		{
			OutSolution.HitResult = MuzzleHit;
			return false;
		}
		FinalActor = MuzzleHit.GetActor();
		FinalComponent = MuzzleHit.GetComponent();
		OutSolution.AimPoint = MuzzleHit.ImpactPoint;
		OutSolution.MuzzleEnd = MuzzleHit.ImpactPoint;
	}

	OutSolution.HitResult = bMuzzleHit ? MuzzleHit : FHitResult();
	OutSolution.TargetActor = FinalActor;
	OutSolution.HitComponent = FinalComponent;
	OutSolution.bWeakpoint = FinalComponent && FinalComponent->IsA<USlimeWeakpointComponent>();
	OutSolution.bHit = FinalActor != nullptr;
	return true;
}

bool USlimeAimAssistComponent::DebugForceLock(
	ASlimeEcologyEnemy* Target,
	bool bWeakpoint)
{
	FCandidate Candidate;
	if (!EvaluateCandidate(
		Target,
		bWeakpoint,
		FVector2D::ZeroVector,
		TNumericLimits<float>::Max(),
		Candidate))
	{
		return false;
	}
	SetLockedCandidate(Candidate);
	return true;
}

bool USlimeAimAssistComponent::DebugEvaluateTarget(
	ASlimeEcologyEnemy* Target,
	bool bWeakpoint,
	float& OutScreenDistance) const
{
	ASlimeWarCharacter* Character = GetOwnerCharacter();
	const APlayerController* PlayerController =
		Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PlayerController)
	{
		return false;
	}
	int32 X = 0;
	int32 Y = 0;
	PlayerController->GetViewportSize(X, Y);
	if (X <= 0 || Y <= 0)
	{
		return false;
	}

	FCandidate Candidate;
	const bool bResult = EvaluateCandidate(
		Target,
		bWeakpoint,
		FVector2D(X * 0.5f, Y * 0.5f),
		TNumericLimits<float>::Max(),
		Candidate);
	OutScreenDistance = bResult ? Candidate.ScreenDistance : -1.f;
	return bResult;
}
