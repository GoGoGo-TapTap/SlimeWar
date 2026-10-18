// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeBodyInteractionComponent.h"

#include "Ecology/SlimeEcologyRules.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USlimeBodyInteractionComponent::USlimeBodyInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeBodyInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	LastLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

const USlimeEcologyRules* USlimeBodyInteractionComponent::GetRules() const
{
	return USlimeEcologyRules::GetFor(this);
}

void USlimeBodyInteractionComponent::ReceiveBodyPush(
	const FVector& Direction,
	float Speed,
	AActor* Source)
{
	const USlimeEcologyRules* Rules = GetRules();
	const UWorld* World = GetWorld();
	if (!Rules || !World || Speed <= 0.f)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (LastPushSource.Get() == Source && Now - LastBodyPushTime < 0.1f)
	{
		return;
	}

	FVector PushDirection = Direction;
	PushDirection.Z = 0.f;
	PushDirection = PushDirection.GetSafeNormal2D();
	if (PushDirection.IsNearlyZero())
	{
		return;
	}

	ExternalVelocity = PushDirection * FMath::Max(Speed, ExternalVelocity.Size2D());
	LastPushSource = Source;
	LastBodyPushTime = Now;
	ForcedFusionWindowRemaining = Rules->ForcedFusionWindow;
}

void USlimeBodyInteractionComponent::ApplyExternalVelocity(
	const FVector& DeltaVelocity,
	AActor* Source)
{
	ExternalVelocity += DeltaVelocity;
	ExternalVelocity.Z = 0.f;
	LastPushSource = Source;
	if (const UWorld* World = GetWorld())
	{
		LastBodyPushTime = World->GetTimeSeconds();
	}
}

void USlimeBodyInteractionComponent::SetAimLocked(bool bLocked, AActor* Source)
{
	if (bLocked)
	{
		AimLockSource = Source ? Source : GetOwner();
	}
	else if (!Source || AimLockSource.Get() == Source)
	{
		AimLockSource.Reset();
	}
}

void USlimeBodyInteractionComponent::ReceiveStun(float Duration)
{
	const float NewDuration = FMath::Max(0.f, Duration);
	if (NewDuration <= 0.f)
	{
		return;
	}

	const float Existing = FMath::Max(StunPending, StunRemaining);
	StunRemaining = 0.f;
	StunPending = FMath::Max(Existing, NewDuration);
	SettleTime = 0.f;
}

ESlimeBodyState USlimeBodyInteractionComponent::GetBodyState() const
{
	if (IsAimLocked())
	{
		return ESlimeBodyState::AimLocked;
	}
	if (StunRemaining > 0.f)
	{
		return ESlimeBodyState::Stunned;
	}
	if (StunPending > 0.f)
	{
		return ESlimeBodyState::StunPending;
	}
	return ESlimeBodyState::Normal;
}

void USlimeBodyInteractionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	ApplyExternalMotion(DeltaTime);

	const FVector NewLocation = Owner->GetActorLocation();
	ObservedSpeed = DeltaTime > KINDA_SMALL_NUMBER
		? FVector::Dist2D(LastLocation, NewLocation) / DeltaTime
		: 0.f;
	LastLocation = NewLocation;

	UpdateStun(DeltaTime);
	ForcedFusionWindowRemaining =
		FMath::Max(0.f, ForcedFusionWindowRemaining - DeltaTime);
}

void USlimeBodyInteractionComponent::ApplyExternalMotion(float DeltaTime)
{
	AActor* Owner = GetOwner();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Owner || !Rules || ExternalVelocity.IsNearlyZero())
	{
		return;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector Delta = ExternalVelocity * DeltaTime;
	FHitResult Hit;
	Owner->AddActorWorldOffset(Delta, true, &Hit, ETeleportType::None);

	if (Hit.bBlockingHit && !Hit.ImpactNormal.IsNearlyZero())
	{
		ExternalVelocity =
			FVector::VectorPlaneProject(ExternalVelocity, Hit.ImpactNormal).GetSafeNormal2D()
			* ExternalVelocity.Size2D();
	}

	const float NewSpeed =
		FMath::Max(0.f, ExternalVelocity.Size2D() - Rules->ExternalVelocityDamping * DeltaTime);
	ExternalVelocity = ExternalVelocity.GetSafeNormal2D() * NewSpeed;
	if (NewSpeed < 1.f)
	{
		ExternalVelocity = FVector::ZeroVector;
	}

	if (ExternalVelocity.SizeSquared2D() > FMath::Square(1.f)
		&& FVector::DistSquared2D(Start, Owner->GetActorLocation()) < 0.01f)
	{
		// Blocked flush against geometry: actual movement is zero, which is what the settle test
		// needs to observe. Keep a small residual so sliding can resume if the plane changes.
		ExternalVelocity *= 0.5f;
	}
}

void USlimeBodyInteractionComponent::UpdateStun(float DeltaTime)
{
	const USlimeEcologyRules* Rules = GetRules();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character
		? Character->GetCharacterMovement()
		: nullptr;
	if (!Rules || !Movement)
	{
		return;
	}

	if (StunRemaining > 0.f)
	{
		StunRemaining = FMath::Max(0.f, StunRemaining - DeltaTime);
		return;
	}

	if (StunPending <= 0.f)
	{
		SettleTime = 0.f;
		return;
	}

	const bool bSettled =
		Movement->IsMovingOnGround() && ObservedSpeed <= Rules->StunStopSpeed;
	SettleTime = bSettled ? SettleTime + DeltaTime : 0.f;

	if (SettleTime >= Rules->StunSettleTime)
	{
		StunRemaining = StunPending;
		StunPending = 0.f;
		SettleTime = 0.f;
		ExternalVelocity = FVector::ZeroVector;
	}
}
