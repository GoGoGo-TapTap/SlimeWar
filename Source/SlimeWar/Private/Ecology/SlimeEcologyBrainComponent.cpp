// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyBrainComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

USlimeEcologyBrainComponent::USlimeEcologyBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeEcologyBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	RefreshCountdown = Rules
		? FMath::FRandRange(0.f, FMath::Max(0.01f, Rules->PerceptionInterval))
		: 0.f;
	RandomStream.Initialize(FMath::Rand());

	if (const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy())
	{
		SetHome(Enemy->GetActivityCenter(), Enemy->GetActivityRadius());
	}
}

ASlimeEcologyEnemy* USlimeEcologyBrainComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEcologyEnemy>(GetOwner());
}

USlimeLocomotionComponent* USlimeEcologyBrainComponent::GetLocomotion() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetLocomotion() : nullptr;
}

USlimeEcologySubsystem* USlimeEcologyBrainComponent::GetEcologySubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USlimeEcologySubsystem>() : nullptr;
}

void USlimeEcologyBrainComponent::SetHome(const FVector& InHome, float InActivityRadius)
{
	HomeLocation = InHome.IsNearlyZero()
		? (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector)
		: InHome;
	ActivityRadius = FMath::Max(0.f, InActivityRadius);
	GroupCenter = HomeLocation;
	bHasExploreGoal = false;

	if (const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy())
	{
		if (Enemy->GetActorLocation().IsNearlyZero())
		{
			HomeLocation = Enemy->GetActorLocation();
		}
	}
}

void USlimeEcologyBrainComponent::ForceRefresh()
{
	RefreshCountdown = 0.f;
}

void USlimeEcologyBrainComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	if (!Enemy)
	{
		return;
	}

	const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
	const USlimeBodyInteractionComponent* Body = Enemy->GetBodyInteraction();
	if (Enemy->IsRunFrozen()
		|| (Health && Health->IsDead())
		|| (Body && Body->BlocksVoluntaryMovement()))
	{
		Behavior = ESlimeEcologyBehavior::Paused;
		DesiredSpeed = 0.f;
		if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
		{
			Locomotion->ClearMoveGoal();
		}
		return;
	}

	RefreshCountdown -= DeltaTime;
	if (RefreshCountdown > 0.f)
	{
		return;
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	RefreshCountdown = Rules ? FMath::Max(0.05f, Rules->PerceptionInterval) : 0.25f;
	RefreshPerception();
	UpdateBehavior();
	++RefreshCount;
}

void USlimeEcologyBrainComponent::RefreshPerception()
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Enemy || !Rules)
	{
		return;
	}

	VisibleAllies.Reset();
	FVector AllyPositionSum = Enemy->GetActorLocation();
	int32 AllyCount = 1;

	TArray<ASlimeEcologyEnemy*> Candidates;
	if (Ecology)
	{
		Ecology->QueryNeighbors(Enemy, Rules->AllyPerceptionRadius, Candidates);
	}

	for (ASlimeEcologyEnemy* Candidate : Candidates)
	{
		const USlimeHealthComponent* Health = Candidate ? Candidate->GetHealthComponent() : nullptr;
		if (!Candidate || Candidate == Enemy || Candidate->GetFaction() != Enemy->GetFaction())
		{
			continue;
		}
		if (Health && Health->IsDead())
		{
			continue;
		}
		if (!HasLineOfSight(Candidate))
		{
			continue;
		}

		VisibleAllies.Add(Candidate);
		AllyPositionSum += Candidate->GetActorLocation();
		++AllyCount;
	}

	GroupCenter = AllyCount > 0 ? AllyPositionSum / static_cast<float>(AllyCount) : HomeLocation;

	CachedPlayer = UGameplayStatics::GetPlayerPawn(this, 0);
	APawn* Player = CachedPlayer.Get();
	if (!Player)
	{
		bPlayerVisible = false;
		PlayerDistance = TNumericLimits<float>::Max();
		return;
	}

	PlayerDistance = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
	const bool bCloseEnough =
		PlayerDistance <= FMath::Max(Rules->PlayerAvoidExitDistance, Rules->PlayerAvoidEnterDistance);
	bPlayerVisible = bCloseEnough && HasLineOfSight(Player);
}

bool USlimeEcologyBrainComponent::HasLineOfSight(const AActor* Target) const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const UWorld* World = GetWorld();
	if (!Enemy || !Target || !World)
	{
		return false;
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const float HeightTolerance = Rules ? Rules->SightHeightTolerance : 200.f;
	if (FMath::Abs(Enemy->GetActorLocation().Z - Target->GetActorLocation().Z) > HeightTolerance)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeEcologySight), false, Enemy);
	QueryParams.AddIgnoredActor(Target);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		Enemy->GetActorLocation(),
		Target->GetActorLocation(),
		ECC_WorldStatic,
		QueryParams);
}

void USlimeEcologyBrainComponent::UpdateBehavior()
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	USlimeLocomotionComponent* Locomotion = GetLocomotion();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Enemy || !Locomotion || !Rules)
	{
		return;
	}

	const bool bWasAvoiding = Behavior == ESlimeEcologyBehavior::AvoidPlayer;
	const bool bShouldAvoid = bPlayerVisible
		&& (bWasAvoiding
			? PlayerDistance < Rules->PlayerAvoidExitDistance
			: PlayerDistance < Rules->PlayerAvoidEnterDistance);

	if (bShouldAvoid)
	{
		const bool bEnteredAvoid = Behavior != ESlimeEcologyBehavior::AvoidPlayer;
		AvoidGoalAge += Rules->PerceptionInterval;

		APawn* Player = CachedPlayer.Get();
		FVector Away = Player
			? Enemy->GetActorLocation() - Player->GetActorLocation()
			: FVector::ForwardVector;
		Away.Z = 0.f;
		Away = Away.GetSafeNormal2D();

		Behavior = ESlimeEcologyBehavior::AvoidPlayer;
		if (bEnteredAvoid
			|| AvoidGoalAge >= 1.f
			|| Locomotion->ConsumeStuckExhausted())
		{
			DesiredGoal = ChooseAvoidGoal(Away);
			AvoidGoalAge = 0.f;
		}
		DesiredSpeed = Rules->PlayerAvoidSpeed;
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
		bHasExploreGoal = false;
		return;
	}

	AvoidGoalAge = 0.f;

	const float GroupDistance =
		FVector::Dist2D(Enemy->GetActorLocation(), GroupCenter);
	const bool bWasRegrouping = Behavior == ESlimeEcologyBehavior::Regroup;
	const bool bShouldRegroup = bWasRegrouping
		? GroupDistance > Rules->GroupEnterDistance
		: GroupDistance > Rules->GroupExitDistance;

	if (bShouldRegroup)
	{
		Behavior = ESlimeEcologyBehavior::Regroup;
		DesiredGoal = GroupCenter;
		DesiredSpeed = Rules->RegroupSpeed;
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
		bHasExploreGoal = false;
		return;
	}

	if (Behavior != ESlimeEcologyBehavior::Explore)
	{
		Behavior = ESlimeEcologyBehavior::Explore;
		bHasExploreGoal = false;
	}

	ExploreGoalAge += Rules->PerceptionInterval;
	const bool bNeedsNewGoal =
		!bHasExploreGoal
		|| Locomotion->ConsumeStuckExhausted()
		|| Locomotion->IsAtGoal()
		|| ExploreGoalAge >= Rules->WanderSelectionInterval;

	if (bNeedsNewGoal)
	{
		ChooseExploreGoal();
	}

	DesiredSpeed = Rules->ExploreSpeed;
	if (bHasExploreGoal)
	{
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
	}
}

void USlimeEcologyBrainComponent::ChooseExploreGoal()
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Enemy || !Rules)
	{
		return;
	}

	const float MaxRadius = ActivityRadius > 0.f
		? FMath::Min(ActivityRadius, Rules->WanderRadiusMax)
		: Rules->WanderRadiusMax;
	const float MinRadius = FMath::Min(
		MaxRadius,
		FMath::Max(Rules->WanderRadiusMin, Rules->WanderMinimumDisplacement));

	for (int32 Attempt = 0; Attempt < Rules->WanderCandidateCount; ++Attempt)
	{
		const float Angle = RandomStream.FRandRange(0.f, 2.f * PI);
		const float Radius = RandomStream.FRandRange(MinRadius, MaxRadius);
		FVector Candidate = HomeLocation + FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			0.f);

		if (ActivityRadius > 0.f)
		{
			const FVector Offset = Candidate - HomeLocation;
			if (Offset.Size2D() > ActivityRadius)
			{
				Candidate = HomeLocation + Offset.GetSafeNormal2D() * ActivityRadius;
			}
		}

		FNavLocation Projected;
		if (NavSystem && NavSystem->ProjectPointToNavigation(
			Candidate,
			Projected,
			FVector(150.f, 150.f, 300.f)))
		{
			Candidate = Projected.Location;
		}
		else
		{
			continue;
		}

		if (FVector::DistSquared2D(Candidate, Enemy->GetActorLocation())
			< FMath::Square(Rules->WanderMinimumDisplacement))
		{
			continue;
		}

		DesiredGoal = Candidate;
		bHasExploreGoal = true;
		ExploreGoalAge = 0.f;
		return;
	}

	DesiredGoal = GroupCenter;
	bHasExploreGoal = true;
	ExploreGoalAge = 0.f;
}

FVector USlimeEcologyBrainComponent::ChooseAvoidGoal(const FVector& AwayDirection) const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const APawn* Player = CachedPlayer.Get();
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Enemy || !Rules || !Player)
	{
		return Enemy ? Enemy->GetActorLocation() + AwayDirection * Rules->AvoidCandidateDistance
			: FVector::ZeroVector;
	}

	const int32 CandidateCount = FMath::Max(1, Rules->AvoidCandidateCount);
	const float StepAngle = PI / 4.f;
	const FVector Current = Enemy->GetActorLocation();
	FVector Best = Current + AwayDirection * Rules->AvoidCandidateDistance;
	float BestScore = -TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < CandidateCount; ++Index)
	{
		const float OffsetIndex = static_cast<float>(Index) - (CandidateCount - 1) * 0.5f;
		FVector Direction = AwayDirection.RotateAngleAxis(OffsetIndex * StepAngle, FVector::UpVector);
		Direction.Z = 0.f;
		Direction = Direction.GetSafeNormal2D();

		FVector Candidate = Current + Direction * Rules->AvoidCandidateDistance;
		if (ActivityRadius > 0.f)
		{
			const FVector Offset = Candidate - HomeLocation;
			if (Offset.Size2D() > ActivityRadius)
			{
				Candidate = HomeLocation + Offset.GetSafeNormal2D() * ActivityRadius;
			}
		}

		FVector Projected = Candidate;
		if (!ProjectToNavigation(Candidate, Projected))
		{
			continue;
		}

		float Crowding = 0.f;
		if (Ecology)
		{
			TArray<ASlimeEcologyEnemy*> Neighbours;
			Ecology->QueryNeighbors(Enemy, 250.f, Neighbours);
			for (const ASlimeEcologyEnemy* Neighbour : Neighbours)
			{
				if (Neighbour
					&& FVector::DistSquared2D(Projected, Neighbour->GetActorLocation())
						< FMath::Square(200.f))
				{
					Crowding += 1.f;
				}
			}
		}

		const float Score =
			FVector::DistSquared2D(Projected, Player->GetActorLocation()) - Crowding * FMath::Square(100.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Projected;
		}
	}

	return Best;
}

bool USlimeEcologyBrainComponent::ProjectToNavigation(
	const FVector& Desired,
	FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem)
	{
		OutLocation = Desired;
		return true;
	}

	FNavLocation Projected;
	if (!NavSystem->ProjectPointToNavigation(
		Desired,
		Projected,
		FVector(150.f, 150.f, 300.f)))
	{
		return false;
	}

	OutLocation = Projected.Location;
	return true;
}

int32 USlimeEcologyBrainComponent::GetVisibleAllyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& Ally : VisibleAllies)
	{
		Count += Ally.IsValid() ? 1 : 0;
	}
	return Count;
}

FVector USlimeEcologyBrainComponent::GetSeparationVector() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const UCapsuleComponent* OwnCapsule = Enemy ? Enemy->GetCapsuleComponent() : nullptr;
	if (!Enemy || !Rules || !OwnCapsule)
	{
		return FVector::ZeroVector;
	}

	FVector Push = FVector::ZeroVector;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAlly : VisibleAllies)
	{
		const ASlimeEcologyEnemy* Ally = WeakAlly.Get();
		const UCapsuleComponent* AllyCapsule = Ally ? Ally->GetCapsuleComponent() : nullptr;
		if (!Ally || !AllyCapsule)
		{
			continue;
		}

		const float DesiredGap =
			OwnCapsule->GetScaledCapsuleRadius()
			+ AllyCapsule->GetScaledCapsuleRadius()
			+ Rules->ExtraSpacing;
		const FVector Delta = Enemy->GetActorLocation() - Ally->GetActorLocation();
		const float Distance = Delta.Size2D();
		if (Distance <= KINDA_SMALL_NUMBER || Distance >= DesiredGap + 100.f)
		{
			continue;
		}

		const float Strength = FMath::Clamp(1.f - Distance / (DesiredGap + 100.f), 0.f, 1.f);
		Push += Delta.GetSafeNormal2D() * Strength;
	}

	Push *= Rules->SeparationWeight;
	if (Push.SizeSquared2D() > FMath::Square(1.5f))
	{
		Push = Push.GetSafeNormal2D() * 1.5f;
	}
	return Push;
}

const TCHAR* USlimeEcologyBrainComponent::GetBehaviorName() const
{
	switch (Behavior)
	{
	case ESlimeEcologyBehavior::Paused:			return TEXT("Paused");
	case ESlimeEcologyBehavior::AvoidPlayer:	return TEXT("AvoidPlayer");
	case ESlimeEcologyBehavior::Regroup:		return TEXT("Regroup");
	case ESlimeEcologyBehavior::Explore:		return TEXT("Explore");
	default:									return TEXT("Unknown");
	}
}
