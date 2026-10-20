// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeLocomotionComponent.h"

#include "Audio/SlimeAudioSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float MinimumPathRefreshCooldown = 0.5f;
	constexpr float LineProbeHeightOffset = 0.f;
}

USlimeLocomotionComponent::USlimeLocomotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeLocomotionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const USlimeEcologyRules* Rules = GetRules())
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Movement->MaxAcceleration = Rules->MoveAcceleration;
			Movement->BrakingDecelerationWalking = Rules->BrakingDeceleration;
			Movement->RotationRate = FRotator(0.f, Rules->TurnRateDegPerSecond, 0.f);
			Movement->bOrientRotationToMovement = true;
		}
	}

	ProgressAnchor = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

ASlimeEcologyEnemy* USlimeLocomotionComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEcologyEnemy>(GetOwner());
}

USlimeEcologyBrainComponent* USlimeLocomotionComponent::GetBrain() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetEcologyBrain() : nullptr;
}

USlimeBodyInteractionComponent* USlimeLocomotionComponent::GetBodyInteraction() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetBodyInteraction() : nullptr;
}

const USlimeEcologyRules* USlimeLocomotionComponent::GetRules() const
{
	return USlimeEcologyRules::GetFor(GetOwner());
}

void USlimeLocomotionComponent::SetMoveGoal(
	const FVector& InGoal,
	float InSpeed,
	float InArrivalRadius)
{
	const bool bNewGoal =
		!bHasGoal || FVector::DistSquared2D(Goal, InGoal) > FMath::Square(25.f);

	DesiredSpeed = FMath::Max(0.f, InSpeed);
	GoalArrivalRadius = InArrivalRadius;
        bAutonomousSuspended = false;
	bHasGoal = true;
	bAtGoal = false;
	bStuckExhausted = false;
	bStopped = false;

	if (bNewGoal || PathPoints.Num() == 0)
	{
                Goal = InGoal;
		PathFailures = 0;
		PathIndex = 0;
		PathRefreshRemaining = 0.f;
		RefreshPath();
	}
}

void USlimeLocomotionComponent::ClearMoveGoal()
{
	bHasGoal = false;
	bAtGoal = false;
	bStuckExhausted = false;
	PathPoints.Reset();
	PathIndex = 0;
	CurrentWaypoint = FVector::ZeroVector;
	SmoothedDirection = FVector::ZeroVector;
	PathFailures = 0;
	GoalArrivalRadius = -1.f;
        bAutonomousSuspended = false;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	bStopped = true;
}

void USlimeLocomotionComponent::SuspendVoluntaryMovement()
{
        if (bAutonomousSuspended)
        {
                return;
        }

        bHasGoal = false;
        bAtGoal = false;
        bStuckExhausted = false;
        PathPoints.Reset();
        PathIndex = 0;
        CurrentWaypoint = FVector::ZeroVector;
        SmoothedDirection = FVector::ZeroVector;
        PathFailures = 0;
        GoalArrivalRadius = -1.f;
        bAutonomousSuspended = true;
        bStopped = true;

        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
                UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
                FVector Velocity = Movement->Velocity;
                Velocity.X = 0.f;
                Velocity.Y = 0.f;
                Movement->Velocity = Velocity;
        }
}

bool USlimeLocomotionComponent::ConsumeStuckExhausted()
{
	const bool bResult = bStuckExhausted;
	bStuckExhausted = false;
	return bResult;
}

void USlimeLocomotionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Enemy || !Character)
	{
		return;
	}

	// Before every early return below: a suspended or frozen slime still has to be able to stop a
	// loop that is already playing.
	UpdateMoveAudio(DeltaTime);

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const USlimeBodyInteractionComponent* Body = GetBodyInteraction();
        if (Body && Body->BlocksVoluntaryMovement())
        {
                SuspendVoluntaryMovement();
                return;
        }

        bAutonomousSuspended = false;
        if (!bHasGoal || Enemy->IsRunFrozen())
	{
		if (!bStopped)
		{
			Movement->StopMovementImmediately();
			bStopped = true;
		}
		return;
	}

	const USlimeEcologyRules* Rules = GetRules();
	if (!Rules)
	{
		return;
	}

	const FVector Location = Enemy->GetActorLocation();
	const float ArrivalRadius = GoalArrivalRadius >= 0.f
		? GoalArrivalRadius
		: Rules->WanderArrivalRadius;
	if (FVector::DistSquared2D(Location, Goal) <= FMath::Square(ArrivalRadius))
	{
		StopAtGoal();
		return;
	}

	PathRefreshRemaining -= DeltaTime;

	while (PathPoints.IsValidIndex(PathIndex))
	{
		const bool bLastWaypoint = PathIndex == PathPoints.Num() - 1;
		const float WaypointRadius = bLastWaypoint
			? FMath::Min(Rules->WaypointAcceptanceRadius, ArrivalRadius)
			: Rules->WaypointAcceptanceRadius;
		if (FVector::DistSquared2D(Location, PathPoints[PathIndex])
			> FMath::Square(WaypointRadius))
		{
			break;
		}
		++PathIndex;
	}

	if (!PathPoints.IsValidIndex(PathIndex))
	{
		if (PathRefreshRemaining <= 0.f && PathFailures <= Rules->MaxPathFailures)
		{
			RefreshPath();
		}

		if (!PathPoints.IsValidIndex(PathIndex))
		{
			return;
		}
	}

	CurrentWaypoint = PathPoints[PathIndex];
	FVector PathDirection = CurrentWaypoint - Location;
	PathDirection.Z = 0.f;
	PathDirection = PathDirection.GetSafeNormal();

	const FVector Separation = GetBrain()
		? GetBrain()->GetSeparationVector()
		: FVector::ZeroVector;
	const FVector LateralSeparation =
		Separation - PathDirection * FVector::DotProduct(Separation, PathDirection);
	FVector CandidateDirection =
		(PathDirection + LateralSeparation * Rules->SeparationOverlayWeight).GetSafeNormal2D();

	if (CandidateDirection.IsNearlyZero())
	{
		Movement->StopMovementImmediately();
		bStopped = true;
		return;
	}

	const float BlendAlpha =
		1.f - FMath::Exp(-FMath::Max(0.1f, Rules->DirectionSmoothing) * DeltaTime);
	SmoothedDirection = FMath::Lerp(SmoothedDirection, CandidateDirection, BlendAlpha)
		.GetSafeNormal2D();

	Movement->MaxWalkSpeed = DesiredSpeed;
	Character->AddMovementInput(SmoothedDirection, 1.f);
	bStopped = false;

	const float MovedDistance = FVector::Dist2D(ProgressAnchor, Location);
	if (MovedDistance >= Rules->StuckDistance)
	{
		ProgressAnchor = Location;
		TimeWithoutProgress = 0.f;
	}
	else
	{
		TimeWithoutProgress += DeltaTime;
		MaxStuckTime = FMath::Max(MaxStuckTime, TimeWithoutProgress);
		if (TimeWithoutProgress >= Rules->StuckCheckSeconds)
		{
			HandleStuck();
		}
	}
}

void USlimeLocomotionComponent::RefreshPath()
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Enemy || !Rules)
	{
		return;
	}

	const FVector Start = Enemy->GetActorLocation();
	PathPoints.Reset();
	PathIndex = 0;
	CurrentWaypoint = Goal;

	if (IsDirectPathClear(Start, Goal))
	{
		PathPoints.Add(Goal);
	}
	else if (!BuildNavPath(Start, Goal, PathPoints))
	{
		++PathFailures;
	}

	++PathRequestCount;
	PathRefreshRemaining = FMath::Max(MinimumPathRefreshCooldown, Rules->PathRefreshInterval);
	ProgressAnchor = Start;
	TimeWithoutProgress = 0.f;
}

bool USlimeLocomotionComponent::IsDirectPathClear(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const UCapsuleComponent* Capsule = Enemy ? Enemy->GetCapsuleComponent() : nullptr;
	if (!World || !Enemy || !Capsule)
	{
		return false;
	}

	FVector Direction = To - From;
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return true;
	}

	const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction);
	const float HalfWidth = Capsule->GetScaledCapsuleRadius() * 0.8f;
	// Probe at the body's current height. Using To.Z directly made every ground-level goal
	// (minerals, delivery pads) end inside the floor and look "blocked", forcing a nav request
	// even when the straight line was clear.
	const float TraceZ = From.Z + LineProbeHeightOffset;
	const FVector TraceStart(From.X, From.Y, TraceZ);
	const FVector TraceEnd(To.X, To.Y, TraceZ);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeDirectPath), false, Enemy);
	for (const float SideScale : {-1.f, 0.f, 1.f})
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(
			Hit,
			TraceStart + Side * HalfWidth * SideScale,
			TraceEnd + Side * HalfWidth * SideScale,
			ECC_WorldStatic,
			QueryParams))
		{
			return false;
		}
	}

	return true;
}

bool USlimeLocomotionComponent::BuildNavPath(
	const FVector& From,
	const FVector& To,
	TArray<FVector>& OutPoints) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;
	if (!NavSystem)
	{
		return false;
	}

	UNavigationPath* Path = NavSystem->FindPathToLocationSynchronously(
		const_cast<USlimeLocomotionComponent*>(this), From, To, nullptr, nullptr);
	if (!Path || !Path->IsValid() || Path->PathPoints.Num() < 2)
	{
		return false;
	}

	OutPoints = Path->PathPoints;
	if (OutPoints.Num() > 1
		&& FVector::DistSquared2D(OutPoints[0], From) <= FMath::Square(25.f))
	{
		OutPoints.RemoveAt(0);
	}
	return OutPoints.Num() > 0;
}

void USlimeLocomotionComponent::HandleStuck()
{
	const USlimeEcologyRules* Rules = GetRules();
	if (!Rules)
	{
		return;
	}

	++PathFailures;
	TimeWithoutProgress = 0.f;

	if (PathFailures > Rules->MaxPathFailures)
	{
		bStuckExhausted = true;
		ClearMoveGoal();
		return;
	}

	RefreshPath();
}

void USlimeLocomotionComponent::StopAtGoal()
{
	bAtGoal = true;
	bHasGoal = false;
	PathPoints.Reset();
	PathIndex = 0;
	SmoothedDirection = FVector::ZeroVector;
	bStopped = true;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}

void USlimeLocomotionComponent::UpdateMoveAudio(float DeltaTime)
{
	USlimeAudioSubsystem* Audio = USlimeAudioSubsystem::Get(this);
	if (!Audio || !GetOwnerEnemy() || !GetOwnerEnemy()->HasActorBegunPlay())
	{
		return;
	}

	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeAudioConfig* Config = Audio->GetConfig();
	const float EnterSpeed = Config ? Config->MoveLoopEnterSpeed : 40.f;
	const float ExitSpeed = Config ? Config->MoveLoopExitSpeed : 15.f;
	const float Dwell = Config ? Config->MoveLoopDwellSeconds : 0.2f;

	// Frozen (run ended) or dead: cut immediately, never fade out into the settlement screen.
	const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
	if (Enemy->IsRunFrozen() || (Health && Health->IsDead()))
	{
		MoveLoopOnSeconds = 0.f;
		MoveLoopOffSeconds = 0.f;
		Audio->StopLoop(MoveLoopPlayingID);
		return;
	}

	// Measured speed, not GetDesiredSpeed(): ClearMoveGoal() leaves the desired speed at its old
	// value, so a slime standing still on purpose would still report full speed.
	const float Speed = Enemy->GetVelocity().Size2D();

	if (MoveLoopPlayingID != USlimeAudioSubsystem::InvalidPlayingID)
	{
		// Hysteresis plus a dwell time: waypoint hand-offs dip the speed for a single frame and must
		// not chop the loop into pieces.
		MoveLoopOffSeconds = (Speed <= ExitSpeed) ? MoveLoopOffSeconds + DeltaTime : 0.f;
		if (MoveLoopOffSeconds >= Dwell)
		{
			Audio->StopLoop(MoveLoopPlayingID);
			MoveLoopOffSeconds = 0.f;
		}
		return;
	}

	MoveLoopOnSeconds = (Speed >= EnterSpeed) ? MoveLoopOnSeconds + DeltaTime : 0.f;
	if (MoveLoopOnSeconds >= Dwell)
	{
		// Returns 0 when out of earshot; the timer simply restarts and tries again next window.
		MoveLoopPlayingID = Audio->PlayLoop(ESlimeAudioEvent::MonsterMove, Enemy);
		MoveLoopOnSeconds = 0.f;
	}
}

void USlimeLocomotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// Covers level unload and merged-away bodies. PostEvent's bStopWhenAttachedToDestroyed is the
	// second line of defence, but an explicit stop also clears the id.
	if (USlimeAudioSubsystem* Audio = USlimeAudioSubsystem::Get(this))
	{
		Audio->StopLoop(MoveLoopPlayingID);
	}

	Super::EndPlay(Reason);
}
