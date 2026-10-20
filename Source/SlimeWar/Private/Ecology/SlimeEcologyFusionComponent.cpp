// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyFusionComponent.h"

#include "Audio/SlimeAudioSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeAlertComponent.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Engine/World.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "NavigationSystem.h"

namespace
{
	float DistancePointToSegment2D(
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd)
	{
		const FVector A(SegmentStart.X, SegmentStart.Y, 0.f);
		const FVector B(SegmentEnd.X, SegmentEnd.Y, 0.f);
		const FVector P(Point.X, Point.Y, 0.f);
		const FVector AB = B - A;
		const float LengthSq = AB.SizeSquared();
		if (LengthSq <= KINDA_SMALL_NUMBER)
		{
			return FVector::Dist2D(P, A);
		}

		const float T = FMath::Clamp(FVector::DotProduct(P - A, AB) / LengthSq, 0.f, 1.f);
		return FVector::Dist2D(P, A + AB * T);
	}

	FVector ProjectMeetingPoint(UWorld* World, const FVector& Midpoint)
	{
		if (!World)
		{
			return Midpoint;
		}

		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation Projected;
			if (Nav->ProjectPointToNavigation(
				Midpoint,
				Projected,
				FVector(200.f, 200.f, 500.f)))
			{
				return Projected.Location;
			}
		}
		return Midpoint;
	}
}

USlimeEcologyFusionComponent::USlimeEcologyFusionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeEcologyFusionComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	ResetAutoReady();
}

ASlimeEcologyEnemy* USlimeEcologyFusionComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEcologyEnemy>(GetOwner());
}

USlimeBodyInteractionComponent* USlimeEcologyFusionComponent::GetBody() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetBodyInteraction() : nullptr;
}

USlimeLocomotionComponent* USlimeEcologyFusionComponent::GetLocomotion() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetLocomotion() : nullptr;
}

USlimeEcologySubsystem* USlimeEcologyFusionComponent::GetEcologySubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USlimeEcologySubsystem>() : nullptr;
}

const USlimeEcologyRules* USlimeEcologyFusionComponent::GetRules() const
{
	return USlimeEcologyRules::GetFor(this);
}

float USlimeEcologyFusionComponent::PickAutoReadyDelay() const
{
	const USlimeEcologyRules* Rules = GetRules();
	const float Min = Rules ? Rules->FusionAutoPrepMin : 14.f;
	const float Max = Rules ? FMath::Max(Min, Rules->FusionAutoPrepMax) : 22.f;
	return FMath::FRandRange(Min, Max);
}

void USlimeEcologyFusionComponent::ResetAutoReady()
{
	AutoReadyRemaining = PickAutoReadyDelay();
}

void USlimeEcologyFusionComponent::ForceReady()
{
	AutoReadyRemaining = 0.f;
}

bool USlimeEcologyFusionComponent::IsEngaged() const
{
	return State == ESlimeEcologyFusionState::Seeking
		|| State == ESlimeEcologyFusionState::Contacting;
}

bool USlimeEcologyFusionComponent::IsPaired() const
{
	return IsEngaged() && Partner.IsValid();
}

bool USlimeEcologyFusionComponent::IsContacting() const
{
	return State == ESlimeEcologyFusionState::Contacting;
}

float USlimeEcologyFusionComponent::GetContactAlpha() const
{
	const USlimeEcologyRules* Rules = GetRules();
	const float Required = Rules ? Rules->FusionContactTime : 2.4f;
	return Required > 0.f ? FMath::Clamp(ContactTime / Required, 0.f, 1.f) : 0.f;
}

float USlimeEcologyFusionComponent::GetContactDistance(const ASlimeEcologyEnemy* Other) const
{
	const ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	const UCapsuleComponent* A = Owner ? Owner->GetCapsuleComponent() : nullptr;
	const UCapsuleComponent* B = Other ? Other->GetCapsuleComponent() : nullptr;
	const USlimeEcologyRules* Rules = GetRules();
	return (A ? A->GetScaledCapsuleRadius() : 0.f)
		+ (B ? B->GetScaledCapsuleRadius() : 0.f)
		+ (Rules ? Rules->FusionContactTolerance : 0.f);
}

float USlimeEcologyFusionComponent::GetForcedCaptureDistance(const ASlimeEcologyEnemy* Other) const
{
	const ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	const UCapsuleComponent* A = Owner ? Owner->GetCapsuleComponent() : nullptr;
	const UCapsuleComponent* B = Other ? Other->GetCapsuleComponent() : nullptr;
	const USlimeEcologyRules* Rules = GetRules();
	return (A ? A->GetScaledCapsuleRadius() : 0.f)
		+ (B ? B->GetScaledCapsuleRadius() : 0.f)
		+ (Rules ? Rules->FusionForcedCaptureGap : 0.f);
}

bool USlimeEcologyFusionComponent::CanRunAutonomous() const
{
	const USlimeBodyInteractionComponent* Body = GetBody();
	return State == ESlimeEcologyFusionState::Idle
		&& AutoReadyRemaining <= 0.f
		&& CooldownRemaining <= 0.f
                && ShotCooldownRemaining <= 0.f
		&& BlockRemaining <= 0.f
		&& SplitCalmRemaining <= 0.f
		&& (!Body || !Body->BlocksVoluntaryMovement());
}

bool USlimeEcologyFusionComponent::CanPairWith(
	const ASlimeEcologyEnemy* Other,
	ESlimeEcologyFusionPath InPath) const
{
	const ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Owner || !Other || Other == Owner || !Rules)
	{
		return false;
	}
        if (Other->GetFaction() != Owner->GetFaction() || CooldownRemaining > 0.f)
	{
		return false;
	}
        if (InPath != ESlimeEcologyFusionPath::Forced && ShotCooldownRemaining > 0.f)
        {
                return false;
        }
	if (InPath != ESlimeEcologyFusionPath::Forced
		&& (Owner->GetCargo() > 0 || Other->GetCargo() > 0))
	{
		return false;
	}
	const USlimeHealthComponent* MyHealth = Owner->GetHealthComponent();
	const USlimeHealthComponent* OtherHealth = Other->GetHealthComponent();
	const USlimeEcologyFusionComponent* OtherFusion = Other->GetEcologyFusionComponent();
	if (!MyHealth || !OtherHealth || MyHealth->IsDead() || OtherHealth->IsDead() || !OtherFusion)
	{
		return false;
	}
	if (SplitCalmRemaining > 0.f || OtherFusion->SplitCalmRemaining > 0.f)
	{
		return false;
	}
	if (OtherFusion == this || OtherFusion->IsEngaged() || OtherFusion->bResolving)
	{
		return false;
	}
        if (InPath == ESlimeEcologyFusionPath::Forced
                && OtherFusion->ShotCooldownRemaining > 0.f
                && !OtherFusion->bShotSnapshotReady)
        {
                return false;
        }

	const int32 TotalMass = Owner->GetMass() + Other->GetMass();
	if (InPath == ESlimeEcologyFusionPath::Forced)
	{
		// Displacement matchmaking is allowed while stun is pending or active. The
		// split-calm timer above and the pre-shot cooldown snapshot are the gates.
		return TotalMass <= Rules->FusionTotalMassCap
			&& CooldownRemaining <= 0.f
			&& OtherFusion->CooldownRemaining <= 0.f
                        && (ShotCooldownRemaining <= 0.f || bShotSnapshotReady)
			&& (OtherFusion->ShotCooldownRemaining <= 0.f || OtherFusion->bShotSnapshotReady);
	}

	const USlimeBodyInteractionComponent* MyBody = GetBody();
	const USlimeBodyInteractionComponent* OtherBody = Other->GetBodyInteraction();
	if ((MyBody && MyBody->BlocksVoluntaryMovement())
		|| (OtherBody && OtherBody->BlocksVoluntaryMovement()))
	{
		return false;
	}
	if (TotalMass > Rules->FusionAutoMassCap)
	{
		return false;
	}
        const bool bTemporaryContactReady =
                InPath == ESlimeEcologyFusionPath::Contact
                && (TemporaryContactReadyRemaining > 0.f
                        || OtherFusion->TemporaryContactReadyRemaining > 0.f);
        if (!bTemporaryContactReady && (BlockRemaining > 0.f || OtherFusion->BlockRemaining > 0.f))
	{
		return false;
	}
	if (InPath == ESlimeEcologyFusionPath::Autonomous
		&& (AutoReadyRemaining > 0.f || OtherFusion->AutoReadyRemaining > 0.f))
	{
		return false;
	}
	return true;
}

bool USlimeEcologyFusionComponent::HasLineOfSight(const ASlimeEcologyEnemy* Other) const
{
	const ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	const UWorld* World = GetWorld();
	if (!Owner || !Other || !World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeEcologyFusionLOS), false, Owner);
	Params.AddIgnoredActor(Other);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		Owner->GetActorLocation(),
		Other->GetActorLocation(),
		ECC_WorldStatic,
		Params);
}

ASlimeEcologyEnemy* USlimeEcologyFusionComponent::FindBestPartner(
	ESlimeEcologyFusionPath InPath) const
{
	const ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Owner || !Ecology || !Rules)
	{
		return nullptr;
	}

	const float SearchRadius = InPath == ESlimeEcologyFusionPath::Autonomous
		? Rules->FusionAutoSeekRange
		: FMath::Max(250.f, GetForcedCaptureDistance(nullptr) + 150.f);
	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Owner, SearchRadius, Neighbours);

	ASlimeEcologyEnemy* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (ASlimeEcologyEnemy* Candidate : Neighbours)
	{
		if (!Candidate || !CanPairWith(Candidate, InPath) || !HasLineOfSight(Candidate))
		{
			continue;
		}

		const float DistanceSq =
			FVector::DistSquared2D(Owner->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}
	return Best;
}

void USlimeEcologyFusionComponent::SetPairCollisionIgnore(
	ASlimeEcologyEnemy* Other,
	bool bIgnore) const
{
	ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	if (!Owner || !Other)
	{
		return;
	}

	if (UCapsuleComponent* A = Owner->GetCapsuleComponent())
	{
		A->IgnoreActorWhenMoving(Other, bIgnore);
	}
	if (UCapsuleComponent* B = Other->GetCapsuleComponent())
	{
		B->IgnoreActorWhenMoving(Owner, bIgnore);
	}
}

void USlimeEcologyFusionComponent::SetState(ESlimeEcologyFusionState NewState)
{
	State = NewState;
}

void USlimeEcologyFusionComponent::SetupLocalPairing(
	ASlimeEcologyEnemy* Other,
	ESlimeEcologyFusionPath InPath,
	const FVector& InMeetingPoint,
	bool InIsInitiator)
{
	Partner = Other;
	Path = InPath;
	MeetingPoint = InMeetingPoint;
	ContactTime = 0.f;
	SeekElapsed = 0.f;
	bInitiator = InIsInitiator;
	SetPairCollisionIgnore(Other, true);
	SetState(InPath == ESlimeEcologyFusionPath::Contact
		? ESlimeEcologyFusionState::Contacting
		: ESlimeEcologyFusionState::Seeking);

	if (USlimeLocomotionComponent* Loco = GetLocomotion())
	{
		if (InPath == ESlimeEcologyFusionPath::Autonomous)
		{
			Loco->SetMoveGoal(MeetingPoint, GetRules()->FusionAutoSeekSpeed, 10.f);
		}
		else
		{
			Loco->ClearMoveGoal();
		}
	}
}

bool USlimeEcologyFusionComponent::AcceptPairing(
	ASlimeEcologyEnemy* Requester,
	ESlimeEcologyFusionPath InPath)
{
	if (!CanPairWith(Requester, InPath))
	{
		return false;
	}

	const FVector Midpoint = (Requester->GetActorLocation() + GetOwnerEnemy()->GetActorLocation()) * 0.5f;
	SetupLocalPairing(
		Requester,
		InPath,
		ProjectMeetingPoint(GetWorld(), Midpoint),
		/*bInitiator=*/false);
	return true;
}

bool USlimeEcologyFusionComponent::BeginPairing(
	ASlimeEcologyEnemy* Other,
	ESlimeEcologyFusionPath InPath)
{
	if (!CanPairWith(Other, InPath))
	{
		return false;
	}

	USlimeEcologyFusionComponent* OtherFusion = Other->GetEcologyFusionComponent();
	if (!OtherFusion || !OtherFusion->AcceptPairing(GetOwnerEnemy(), InPath))
	{
		return false;
	}

	const FVector Midpoint =
		(GetOwnerEnemy()->GetActorLocation() + Other->GetActorLocation()) * 0.5f;
	SetupLocalPairing(
		Other,
		InPath,
		ProjectMeetingPoint(GetWorld(), Midpoint),
		/*bInitiator=*/true);

	// Fusion begins. Only the initiator reaches this line - the partner goes through AcceptPairing -
	// so a pair cannot produce two start sounds.
	if (USlimeAudioSubsystem* Audio = USlimeAudioSubsystem::Get(this))
	{
		Audio->PlayGatedEvent(ESlimeAudioEvent::MonsterMerge, GetOwnerEnemy());
	}

	return true;
}

bool USlimeEcologyFusionComponent::TryBeginAutonomousFusion()
{
	if (!CanRunAutonomous())
	{
		return false;
	}
	if (ASlimeEcologyEnemy* Candidate = FindBestPartner(ESlimeEcologyFusionPath::Autonomous))
	{
		return BeginPairing(Candidate, ESlimeEcologyFusionPath::Autonomous);
	}
	return false;
}

bool USlimeEcologyFusionComponent::TryBeginContactFusion()
{
        if (State != ESlimeEcologyFusionState::Idle
                || CooldownRemaining > 0.f
                || ShotCooldownRemaining > 0.f
                || BlockRemaining > 0.f)
	{
		return false;
	}

	if (ASlimeEcologyEnemy* Candidate = FindBestPartner(ESlimeEcologyFusionPath::Contact))
	{
		const float Distance =
			FVector::Dist2D(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= GetContactDistance(Candidate))
		{
			return BeginPairing(Candidate, ESlimeEcologyFusionPath::Contact);
		}
	}
	return false;
}

bool USlimeEcologyFusionComponent::CanJoinForced(const ASlimeEcologyEnemy* Other) const
{
	return CanPairWith(Other, ESlimeEcologyFusionPath::Forced);
}

void USlimeEcologyFusionComponent::TryForcedFusion()
{
	ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	USlimeBodyInteractionComponent* Body = GetBody();
	if (!Owner || !Body
		|| (State != ESlimeEcologyFusionState::Idle
			&& State != ESlimeEcologyFusionState::Blocked)
		|| CooldownRemaining > 0.f || Body->GetForcedFusionWindowRemaining() <= 0.f)
	{
		return;
	}

	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Ecology)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Owner, 250.f, Neighbours);
	for (ASlimeEcologyEnemy* Candidate : Neighbours)
	{
		if (!Candidate || !CanJoinForced(Candidate) || !HasLineOfSight(Candidate))
		{
			continue;
		}

		const float SweptDistance = DistancePointToSegment2D(
			Candidate->GetActorLocation(),
			PreviousLocation,
			Owner->GetActorLocation());
		if (SweptDistance <= GetForcedCaptureDistance(Candidate))
		{
			ResolveForcedPair(Candidate);
			return;
		}
	}
}

void USlimeEcologyFusionComponent::TickSearch(float DeltaTime)
{
	TryForcedFusion();
	if (State != ESlimeEcologyFusionState::Idle)
	{
		return;
	}

	SearchAccumulator += DeltaTime;
	if (SearchAccumulator < 0.25f)
	{
		return;
	}
	SearchAccumulator = 0.f;

	if (!TryBeginContactFusion())
	{
		TryBeginAutonomousFusion();
	}
}

void USlimeEcologyFusionComponent::TickPairing(float DeltaTime)
{
	ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	ASlimeEcologyEnemy* Other = Partner.Get();
	USlimeEcologyFusionComponent* OtherFusion =
		Other ? Other->GetEcologyFusionComponent() : nullptr;
	const USlimeEcologyRules* Rules = GetRules();
	if (!Owner || !Other || !OtherFusion || !Rules || Other->IsActorBeingDestroyed())
	{
		CancelPairing(ESlimeEcologyFusionEndReason::PartnerLost, 0.f);
		return;
	}

	const USlimeBodyInteractionComponent* Body = GetBody();
	if (Body && (Body->IsStunPending() || Body->IsStunned()))
	{
		CancelPairing(ESlimeEcologyFusionEndReason::Interrupted);
		return;
	}

	const float Distance =
		FVector::Dist2D(Owner->GetActorLocation(), Other->GetActorLocation());
	if (State == ESlimeEcologyFusionState::Seeking)
	{
		if (Body && Body->IsAimLocked())
		{
			CancelPairing(ESlimeEcologyFusionEndReason::Interrupted);
			return;
		}

		SeekElapsed += DeltaTime;
		if (SeekElapsed > Rules->FusionSeekTimeout)
		{
			CancelPairing(ESlimeEcologyFusionEndReason::Timeout);
			return;
		}

		if (Distance <= GetContactDistance(Other))
		{
			SetState(ESlimeEcologyFusionState::Contacting);
			ContactTime = 0.f;
			if (USlimeLocomotionComponent* Loco = GetLocomotion())
			{
				Loco->ClearMoveGoal();
			}
		}
		else
		{
			MeetingPoint = ProjectMeetingPoint(
				GetWorld(),
				(Owner->GetActorLocation() + Other->GetActorLocation()) * 0.5f);
			if (USlimeLocomotionComponent* Loco = GetLocomotion())
			{
                                Loco->SetMoveGoal(MeetingPoint, Rules->FusionAutoSeekSpeed, 10.f);
			}
		}
	}

	if (State == ESlimeEcologyFusionState::Contacting)
	{
		if (Distance > GetContactDistance(Other) + 10.f)
		{
			CancelPairing(ESlimeEcologyFusionEndReason::Separated);
			return;
		}

		ContactTime += DeltaTime;
		if (bInitiator && ContactTime >= Rules->FusionContactTime)
		{
			ResolvePairing();
		}
	}
}

void USlimeEcologyFusionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	if (!Owner)
	{
		return;
	}

	CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);
        ShotCooldownRemaining = FMath::Max(0.f, ShotCooldownRemaining - DeltaTime);
        if (ShotCooldownRemaining <= 0.f)
        {
                bShotSnapshotReady = false;
        }
	BlockRemaining = FMath::Max(0.f, BlockRemaining - DeltaTime);
	if (!GetBody() || !GetBody()->IsStunned())
	{
		SplitCalmRemaining = FMath::Max(0.f, SplitCalmRemaining - DeltaTime);
	}
        TemporaryContactReadyRemaining =
                FMath::Max(0.f, TemporaryContactReadyRemaining - DeltaTime);
	if (State == ESlimeEcologyFusionState::Cooldown && CooldownRemaining <= 0.f)
	{
		SetState(ESlimeEcologyFusionState::Idle);
	}
	if (State == ESlimeEcologyFusionState::Blocked && BlockRemaining <= 0.f)
	{
		SetState(ESlimeEcologyFusionState::Idle);
	}

	if (!IsEngaged())
	{
		AutoReadyRemaining = FMath::Max(0.f, AutoReadyRemaining - DeltaTime);
	}

	if (IsEngaged())
	{
		TickPairing(DeltaTime);
	}
	else
	{
		TickSearch(DeltaTime);
	}

	PreviousLocation = Owner->GetActorLocation();
}

void USlimeEcologyFusionComponent::ClearPairing()
{
	ASlimeEcologyEnemy* Owner = GetOwnerEnemy();
	ASlimeEcologyEnemy* Other = Partner.Get();
	if (Owner && Other)
	{
		SetPairCollisionIgnore(Other, false);
	}
	Partner.Reset();
	Path = ESlimeEcologyFusionPath::None;
	ContactTime = 0.f;
	SeekElapsed = 0.f;
	bInitiator = false;
	if (Owner)
	{
		if (USlimeLocomotionComponent* Loco = GetLocomotion())
		{
			Loco->ClearMoveGoal();
		}
		if (USlimeEcologyBrainComponent* Brain = Owner->GetEcologyBrain())
		{
			Brain->ForceRefresh();
		}
	}
}

void USlimeEcologyFusionComponent::CancelPairing(
	ESlimeEcologyFusionEndReason Reason,
	float BlockSeconds)
{
	if (!IsEngaged())
	{
		return;
	}

	const USlimeEcologyRules* Rules = GetRules();
	const float Block = BlockSeconds >= 0.f
		? BlockSeconds
		: (Reason == ESlimeEcologyFusionEndReason::PartnerLost
			? 0.f
			: (Rules ? Rules->FusionInterruptedBlock : 5.f));

	ASlimeEcologyEnemy* Other = Partner.Get();
	if (Other)
	{
		if (USlimeEcologyFusionComponent* OtherFusion = Other->GetEcologyFusionComponent())
		{
			if (OtherFusion != this && !OtherFusion->bResolving)
			{
				OtherFusion->bResolving = true;
				OtherFusion->SetPairCollisionIgnore(GetOwnerEnemy(), false);
				OtherFusion->Partner.Reset();
				OtherFusion->Path = ESlimeEcologyFusionPath::None;
				OtherFusion->ContactTime = 0.f;
				OtherFusion->BlockRemaining = Block;
				OtherFusion->CooldownRemaining = 0.f;
				OtherFusion->ResetAutoReady();
				OtherFusion->SetState(
					Block > 0.f
						? ESlimeEcologyFusionState::Blocked
						: ESlimeEcologyFusionState::Idle);
				OtherFusion->bResolving = false;
				if (USlimeLocomotionComponent* OtherLoco = Other->GetLocomotion())
				{
					OtherLoco->ClearMoveGoal();
				}
				if (USlimeEcologyBrainComponent* OtherBrain = Other->GetEcologyBrain())
				{
					OtherBrain->ForceRefresh();
				}
			}
		}
	}

	ClearPairing();
	BlockRemaining = Block;
	CooldownRemaining = 0.f;
	ResetAutoReady();
	SetState(Block > 0.f
		? ESlimeEcologyFusionState::Blocked
		: ESlimeEcologyFusionState::Idle);
}

void USlimeEcologyFusionComponent::ResolvePairing()
{
	if (bResolving)
	{
		return;
	}

	ASlimeEcologyEnemy* Survivor = GetOwnerEnemy();
	ASlimeEcologyEnemy* Other = Partner.Get();
	USlimeEcologyFusionComponent* OtherFusion =
		Other ? Other->GetEcologyFusionComponent() : nullptr;
	const USlimeHealthComponent* MyHealth = Survivor ? Survivor->GetHealthComponent() : nullptr;
	const USlimeHealthComponent* OtherHealth = Other ? Other->GetHealthComponent() : nullptr;
	const USlimeEcologyRules* Rules = GetRules();
	if (!Survivor || !Other || !OtherFusion || !MyHealth || !OtherHealth || !Rules)
	{
		CancelPairing(ESlimeEcologyFusionEndReason::PartnerLost, 0.f);
		return;
	}

	const int32 NewMass = FMath::Min(
		Rules->FusionTotalMassCap,
		Survivor->GetMass() + Other->GetMass());
	const float HealthFraction = MyHealth->GetMaxHealth() + OtherHealth->GetMaxHealth() > 0.f
		? (MyHealth->GetHealth() + OtherHealth->GetHealth())
			/ (MyHealth->GetMaxHealth() + OtherHealth->GetMaxHealth())
		: 1.f;
	const int32 NewCargo = Survivor->GetCargo() + Other->GetCargo();
	const int32 NewCredit =
		Survivor->GetBodyScoreCredit() + Other->GetBodyScoreCredit();
        const bool bHostileCore =
            (Survivor->GetCoreComponent() && Survivor->GetCoreComponent()->IsHostileCore())
            || (Other->GetCoreComponent() && Other->GetCoreComponent()->IsHostileCore())
            || (Survivor->GetAlertComponent() && Survivor->GetAlertComponent()->IsAlerted())
            || (Other->GetAlertComponent() && Other->GetAlertComponent()->IsAlerted());
        const USlimeBodyInteractionComponent* SurvivorBody = Survivor->GetBodyInteraction();
        const USlimeBodyInteractionComponent* OtherBody = Other->GetBodyInteraction();
        const float CombinedStun = FMath::Max(
                SurvivorBody
                        ? FMath::Max(SurvivorBody->GetStunPending(), SurvivorBody->GetStunRemaining())
                        : 0.f,
                OtherBody
                        ? FMath::Max(OtherBody->GetStunPending(), OtherBody->GetStunRemaining())
                        : 0.f);

	// Fusion committed. Both bodies are still alive here; Other is destroyed a few lines below, so
	// the sound has to be posted on the survivor to outlive it.
	if (USlimeAudioSubsystem* Audio = USlimeAudioSubsystem::Get(this))
	{
		Audio->PlayGatedEvent(ESlimeAudioEvent::MonsterMergeEnd, Survivor);
	}

	bResolving = true;
	OtherFusion->bResolving = true;
	SetPairCollisionIgnore(Other, false);
	OtherFusion->Partner.Reset();
	OtherFusion->Path = ESlimeEcologyFusionPath::None;
	OtherFusion->SetState(ESlimeEcologyFusionState::Idle);
	LastFusionPartner = Other;
	Other->Destroy();
	OtherFusion->bResolving = false;
	bResolving = false;

	Partner.Reset();
	Path = ESlimeEcologyFusionPath::None;
	ContactTime = 0.f;
	Survivor->ApplyStatRow(NewMass, HealthFraction);
	Survivor->SetEcologyTotals(NewCargo, NewCredit);
        if (CombinedStun > 0.f && Survivor->GetBodyInteraction())
        {
                Survivor->GetBodyInteraction()->ReceiveStun(CombinedStun);
        }
        if (USlimeCoreComponent* Core = Survivor->GetCoreComponent())
        {
                Core->ArmCore(bHostileCore
                        ? ESlimeCoreState::Hostile
                        : ESlimeCoreState::Neutral);
        }
        if (USlimeAlertComponent* Alert = Survivor->GetAlertComponent())
        {
                Alert->ClearAlert();
        }
	Survivor->GetBodyInteraction()->ClearForcedFusionWindow();

	CooldownRemaining = Rules->FusionSuccessCooldown;
	ResetAutoReady();
	SetState(ESlimeEcologyFusionState::Cooldown);
	++FusionCount;

	if (USlimeLocomotionComponent* Loco = GetLocomotion())
	{
		Loco->ClearMoveGoal();
	}
	if (USlimeEcologyBrainComponent* Brain = Survivor->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(Survivor))
	{
                FSlimeFusionEvent Event;
                Event.ResultMass = NewMass;
                Event.Cargo = NewCargo;
                Event.BodyScoreCredit = NewCredit;
                Event.bHostileCore = bHostileCore;
                Event.Location = Survivor->GetActorLocation();
                GameMode->OnEnemyFused(Event);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("EcologyFusion: %s + %s -> mass %d, health %.1f/%.1f, cargo %d, credit %d."),
		*GetNameSafe(Survivor),
		*GetNameSafe(Other),
		NewMass,
		Survivor->GetHealthComponent()->GetHealth(),
		Survivor->GetHealthComponent()->GetMaxHealth(),
		NewCargo,
		NewCredit);
}

void USlimeEcologyFusionComponent::ResolveForcedPair(ASlimeEcologyEnemy* Other)
{
	if (!CanJoinForced(Other))
	{
		return;
	}

	ASlimeEcologyEnemy* Survivor = GetOwnerEnemy();
	const USlimeHealthComponent* MyHealth = Survivor->GetHealthComponent();
	const USlimeHealthComponent* OtherHealth = Other->GetHealthComponent();
	const USlimeEcologyRules* Rules = GetRules();
	if (!MyHealth || !OtherHealth || !Rules)
	{
		return;
	}

	const int32 NewMass = FMath::Min(
		Rules->FusionTotalMassCap,
		Survivor->GetMass() + Other->GetMass());
	const float HealthFraction =
		(MyHealth->GetHealth() + OtherHealth->GetHealth())
		/ FMath::Max(1.f, MyHealth->GetMaxHealth() + OtherHealth->GetMaxHealth());
	const int32 NewCargo = Survivor->GetCargo() + Other->GetCargo();
	const int32 NewCredit = Survivor->GetBodyScoreCredit() + Other->GetBodyScoreCredit();
        const bool bHostileCore =
            (Survivor->GetCoreComponent() && Survivor->GetCoreComponent()->IsHostileCore())
            || (Other->GetCoreComponent() && Other->GetCoreComponent()->IsHostileCore())
            || (Survivor->GetAlertComponent() && Survivor->GetAlertComponent()->IsAlerted())
            || (Other->GetAlertComponent() && Other->GetAlertComponent()->IsAlerted());
        const USlimeBodyInteractionComponent* SurvivorBody = Survivor->GetBodyInteraction();
        const USlimeBodyInteractionComponent* OtherBody = Other->GetBodyInteraction();
        const float CombinedStun = FMath::Max(
                SurvivorBody
                        ? FMath::Max(SurvivorBody->GetStunPending(), SurvivorBody->GetStunRemaining())
                        : 0.f,
                OtherBody
                        ? FMath::Max(OtherBody->GetStunPending(), OtherBody->GetStunRemaining())
                        : 0.f);

	USlimeEcologyFusionComponent* OtherFusion = Other->GetEcologyFusionComponent();

	// Second commit path: the player shoved one slime into another. Three contact merges and one
	// shove merge are different code, and only covering ResolvePairing would leave shoves silent.
	if (USlimeAudioSubsystem* Audio = USlimeAudioSubsystem::Get(this))
	{
		Audio->PlayGatedEvent(ESlimeAudioEvent::MonsterMergeEnd, Survivor);
	}

	bResolving = true;
	if (OtherFusion)
	{
		OtherFusion->bResolving = true;
		SetPairCollisionIgnore(Other, false);
		OtherFusion->Partner.Reset();
		OtherFusion->Path = ESlimeEcologyFusionPath::None;
		OtherFusion->SetState(ESlimeEcologyFusionState::Idle);
	}
	LastFusionPartner = Other;
	Other->Destroy();
	if (OtherFusion)
	{
		OtherFusion->bResolving = false;
	}
	bResolving = false;

	Survivor->ApplyStatRow(NewMass, HealthFraction);
	Survivor->SetEcologyTotals(NewCargo, NewCredit);
        if (CombinedStun > 0.f && Survivor->GetBodyInteraction())
        {
                Survivor->GetBodyInteraction()->ReceiveStun(CombinedStun);
        }
        if (USlimeCoreComponent* Core = Survivor->GetCoreComponent())
        {
                Core->ArmCore(bHostileCore
                        ? ESlimeCoreState::Hostile
                        : ESlimeCoreState::Neutral);
        }
        if (USlimeAlertComponent* Alert = Survivor->GetAlertComponent())
        {
                Alert->ClearAlert();
        }
	Survivor->GetBodyInteraction()->ClearForcedFusionWindow();
	CooldownRemaining = Rules->FusionSuccessCooldown;
	ResetAutoReady();
	SetState(ESlimeEcologyFusionState::Cooldown);
	++FusionCount;

	if (USlimeEcologyBrainComponent* Brain = Survivor->GetEcologyBrain())
	{
		Brain->ForceRefresh();
	}
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(Survivor))
	{
                FSlimeFusionEvent Event;
                Event.ResultMass = NewMass;
                Event.Cargo = NewCargo;
                Event.BodyScoreCredit = NewCredit;
                Event.bHostileCore = bHostileCore;
                Event.Location = Survivor->GetActorLocation();
                GameMode->OnEnemyFused(Event);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("EcologyFusion: displacement %s + %s -> mass %d, health %.1f/%.1f, cargo %d, credit %d."),
		*GetNameSafe(Survivor),
		*GetNameSafe(Other),
		NewMass,
		Survivor->GetHealthComponent()->GetHealth(),
		Survivor->GetHealthComponent()->GetMaxHealth(),
		NewCargo,
		NewCredit);
}

bool USlimeEcologyFusionComponent::DebugForcePair(ASlimeEcologyEnemy* Other)
{
	if (!Other)
	{
		return false;
	}
	ForceReady();
	if (USlimeEcologyFusionComponent* F = Other->GetEcologyFusionComponent())
	{
		F->ForceReady();
	}
	return BeginPairing(Other, ESlimeEcologyFusionPath::Autonomous);
}

bool USlimeEcologyFusionComponent::DebugForcedPair(ASlimeEcologyEnemy* Other)
{
	if (!Other)
	{
		return false;
	}
	ResolveForcedPair(Other);
	return FusionCount > 0;
}

void USlimeEcologyFusionComponent::DebugSetSuccessCooldown(float Seconds)
{
	CooldownRemaining = FMath::Max(0.f, Seconds);
	if (CooldownRemaining > 0.f)
	{
		SetState(ESlimeEcologyFusionState::Cooldown);
	}
}

void USlimeEcologyFusionComponent::InterruptFusion(float BlockSeconds)
{
	const float Block = BlockSeconds >= 0.f
		? BlockSeconds
		: (GetRules() ? GetRules()->FusionInterruptedBlock : 5.f);
	if (IsEngaged())
	{
		CancelPairing(ESlimeEcologyFusionEndReason::Interrupted, Block);
	}
	else
	{
		BlockRemaining = Block;
		ResetAutoReady();
		SetState(Block > 0.f ? ESlimeEcologyFusionState::Blocked : ESlimeEcologyFusionState::Idle);
	}
}

void USlimeEcologyFusionComponent::ApplyShotFusionInterference(float CooldownSeconds)
{
        const USlimeEcologyRules* Rules = GetRules();
        if (ShotCooldownRemaining <= 0.f)
        {
                bShotSnapshotReady = CooldownRemaining <= 0.f;
        }

        const float Duration = CooldownSeconds >= 0.f
                ? CooldownSeconds
                : (Rules ? Rules->ShotFusionCooldown : 0.8f);
        ShotCooldownRemaining = FMath::Max(ShotCooldownRemaining, Duration);
}

void USlimeEcologyFusionComponent::ApplySplitCalm(float Seconds)
{
	SplitCalmRemaining = FMath::Max(SplitCalmRemaining, FMath::Max(0.f, Seconds));
}

void USlimeEcologyFusionComponent::ResetForAssimilation()
{
	if (IsEngaged() || IsPaired())
	{
		CancelPairing(ESlimeEcologyFusionEndReason::Interrupted, 0.f);
	}
	ClearPairing();
	State = ESlimeEcologyFusionState::Idle;
	Path = ESlimeEcologyFusionPath::None;
	CooldownRemaining = 0.f;
        ShotCooldownRemaining = 0.f;
        bShotSnapshotReady = false;
	BlockRemaining = 0.f;
	SplitCalmRemaining = 0.f;
	ContactTime = 0.f;
	SeekElapsed = 0.f;
}

void USlimeEcologyFusionComponent::SetTemporaryContactReady(float Seconds)
{
        TemporaryContactReadyRemaining =
                FMath::Max(TemporaryContactReadyRemaining, FMath::Max(0.f, Seconds));
}

void USlimeEcologyFusionComponent::NotifyPartnerDied()
{
	if (IsEngaged())
	{
		CancelPairing(ESlimeEcologyFusionEndReason::PartnerLost, 0.f);
	}
}

void USlimeEcologyFusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyPartnerDied();
	Super::EndPlay(EndPlayReason);
}
