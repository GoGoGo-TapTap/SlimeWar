// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeFusionComponent.h"

#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	const USlimeRunConfig* GetRunConfigFor(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UStatTableProvider* Provider = GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
		return Provider ? Provider->GetRunConfig() : nullptr;
	}

	/**
	 * Both sides walk to the same point: the midpoint between them, dropped onto the navmesh so
	 * nobody tries to path through a wall (design 4.2 "朝彼此之间的空地接近，不能穿过墙体").
	 * Both ends of a pair use the initiator's result, so they literally share one destination.
	 */
	FVector ComputeMeetingPoint(UWorld* World, const AActor& A, const AActor& B)
	{
		const FVector Midpoint = (A.GetActorLocation() + B.GetActorLocation()) * 0.5f;
		if (!World)
		{
			return Midpoint;
		}

		UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavigationSystem)
		{
			return Midpoint;
		}

		FNavLocation Projected;
		if (NavigationSystem->ProjectPointToNavigation(Midpoint, Projected, FVector(200.f, 200.f, 500.f)))
		{
			return Projected.Location;
		}

		return Midpoint;
	}
}

USlimeFusionComponent::USlimeFusionComponent()
{
	// The StateTree "Hold" task drives this component; it must not tick on its own, otherwise
	// 180 slimes would each pay a per-frame update just to idle.
	PrimaryComponentTick.bCanEverTick = false;
}

ASlimeEnemyBase* USlimeFusionComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEnemyBase>(GetOwner());
}

const USlimeRunConfig* USlimeFusionComponent::GetRunConfig() const
{
	return GetRunConfigFor(this);
}

int32 USlimeFusionComponent::GetMassCap() const
{
	const USlimeRunConfig* Config = GetRunConfig();
	return Config ? Config->AIFusionMassCap : 0;
}

int32 USlimeFusionComponent::GetMaxParticipantsFromConfig() const
{
	const USlimeRunConfig* Config = GetRunConfig();
	return Config ? FMath::Max(2, Config->FusionMaxParticipants) : 2;
}

bool USlimeFusionComponent::IsEngaged() const
{
	return State == ESlimeFusionState::Approaching
		|| State == ESlimeFusionState::Contacting
		|| State == ESlimeFusionState::Cooling;
}

bool USlimeFusionComponent::IsPaired() const
{
	return (State == ESlimeFusionState::Approaching || State == ESlimeFusionState::Contacting)
		&& IsValid(Partner.Get());
}

bool USlimeFusionComponent::CanStartPairing() const
{
	return State == ESlimeFusionState::Idle;
}

float USlimeFusionComponent::GetContactAlpha() const
{
	const USlimeRunConfig* Config = GetRunConfig();
	const float Required = Config ? Config->FusionContactTime : 0.f;
	return Required > 0.f ? FMath::Clamp(ContactTime / Required, 0.f, 1.f) : 0.f;
}

float USlimeFusionComponent::GetContactDistance(const ASlimeEnemyBase* Other) const
{
	const ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner || !Other)
	{
		return 0.f;
	}

	const UCapsuleComponent* OwnerCapsule = Owner->GetCapsuleComponent();
	const UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent();
	const float RadiusSum =
		(OwnerCapsule ? OwnerCapsule->GetScaledCapsuleRadius() : 0.f) +
		(OtherCapsule ? OtherCapsule->GetScaledCapsuleRadius() : 0.f);

	const USlimeRunConfig* Config = GetRunConfig();
	const float Tolerance = Config ? Config->FusionContactTolerance : 0.f;
	return RadiusSum + FMath::Max(0.f, Tolerance);
}

float USlimeFusionComponent::GetApproachAcceptanceRadius() const
{
	const USlimeRunConfig* Config = GetRunConfig();

	// Tight on purpose: the two slimes have to end up on (nearly) the same spot to be inside
	// GetContactDistance(). 1 cm is only a sanity floor for a misconfigured 0.
	return FMath::Max(Config ? Config->FusionContactTolerance : 0.f, 1.f);
}

void USlimeFusionComponent::SetState(ESlimeFusionState NewState, ESlimeFusionCancelReason Reason)
{
	if (State == NewState && LastCancelReason == Reason)
	{
		return;
	}

	State = NewState;
	LastCancelReason = Reason;
	OnFusionStateChanged.Broadcast(NewState, Reason);
}

bool USlimeFusionComponent::CanPairWith(const ASlimeEnemyBase* Other) const
{
	const ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner || !Other || Other == Owner)
	{
		return false;
	}

	if (Owner->IsAggressive() || Other->IsAggressive())
	{
		return false;
	}

	if (Other->GetPointId() != Owner->GetPointId())
	{
		return false;
	}

	const USlimeFusionComponent* OtherFusion = Other->GetFusionComponent();
	if (!OtherFusion || OtherFusion->IsEngaged())
	{
		// Already in a pair (or waiting out a cooldown): this is what makes "three slimes
		// touching" resolve to exactly one pair, and stops a slime joining two fusions.
		return false;
	}

	const USlimeHealthComponent* OtherHealth = Other->GetHealthComponent();
	if (!OtherHealth || OtherHealth->IsDead())
	{
		return false;
	}

	const int32 MassCap = GetMassCap();
	if (MassCap > 0 && Owner->GetMass() + Other->GetMass() > MassCap)
	{
		return false;
	}

	// N-way hook: today this is always 2.
	if (Participants.Num() >= GetMaxParticipantsFromConfig())
	{
		return false;
	}

	return true;
}

void USlimeFusionComponent::SetPairCollisionIgnore(ASlimeEnemyBase* Other, bool bIgnore) const
{
	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner || !Other)
	{
		return;
	}

	// Movement level ignore only: the capsules keep blocking the world, the player, the
	// aggressive slimes and the weapon trace (ECC_Visibility), so a fusing pair can be shot.
	if (UCapsuleComponent* OwnerCapsule = Owner->GetCapsuleComponent())
	{
		OwnerCapsule->IgnoreActorWhenMoving(Other, bIgnore);
	}

	if (UCapsuleComponent* OtherCapsule = Other->GetCapsuleComponent())
	{
		OtherCapsule->IgnoreActorWhenMoving(Owner, bIgnore);
	}
}

void USlimeFusionComponent::SetupLocalPairing(ASlimeEnemyBase* Other, const FVector& InMeetingPoint, bool bInitiator)
{
	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner || !Other)
	{
		return;
	}

	Partner = Other;
	Participants.Reset();
	Participants.Add(Owner);
	if (Other != Owner)
	{
		Participants.Add(Other);
	}

	MeetingPoint = InMeetingPoint;
	ContactTime = 0.f;
	ApproachTime = 0.f;
	ClosestGap = TNumericLimits<float>::Max();
	CooldownRemaining = 0.f;
	bIsInitiator = bInitiator;

	Owner->SetFusionTarget(Other);
	SetPairCollisionIgnore(Other, true);

	// A player standing on top of the pair must not be able to shove either of them while they
	// walk into each other.
	Owner->IgnorePlayerForMovement();
	Other->IgnorePlayerForMovement();

	if (USlimeStateComponent* StateComponent = Owner->GetStateComponent())
	{
		StateComponent->RemoveStateTag(TAG_State_Enemy_Normal_Idle);
		StateComponent->AddStateTag(TAG_State_Enemy_Normal_Fusing);
	}

	SetState(ESlimeFusionState::Approaching, ESlimeFusionCancelReason::None);

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("[%s] fusion pairing with %s (mass %d + %d, %s)"),
			*GetNameSafe(Owner), *GetNameSafe(Other), Owner->GetMass(), Other->GetMass(),
			bInitiator ? TEXT("initiator") : TEXT("acceptor"));
	}
}

bool USlimeFusionComponent::BeginPairing(ASlimeEnemyBase* Candidate)
{
	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	UWorld* World = GetWorld();

	if (!Owner || !World || !CanStartPairing() || !CanPairWith(Candidate))
	{
		return false;
	}

	USlimeFusionComponent* CandidateFusion = Candidate->GetFusionComponent();
	if (!CandidateFusion)
	{
		return false;
	}

	const FVector SharedMeetingPoint = ComputeMeetingPoint(World, *Owner, *Candidate);

	// Request -> answer. The candidate validates on its side too, so a request can never take
	// a slime that someone else already reserved.
	if (!CandidateFusion->RespondToRequest(Owner, SharedMeetingPoint))
	{
		return false;
	}

	SetupLocalPairing(Candidate, SharedMeetingPoint, /*bInitiator=*/true);
	return true;
}

bool USlimeFusionComponent::RespondToRequest(ASlimeEnemyBase* Requester, const FVector& InMeetingPoint)
{
	if (!CanStartPairing() || !CanPairWith(Requester))
	{
		return false;
	}

	SetupLocalPairing(Requester, InMeetingPoint, /*bInitiator=*/false);
	return true;
}

bool USlimeFusionComponent::DebugForcePair(ASlimeEnemyBase* Other)
{
	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner || !Other || Other->IsAggressive() || Owner->IsAggressive())
	{
		return false;
	}

	USlimeFusionComponent* OtherFusion = Other->GetFusionComponent();
	if (!OtherFusion)
	{
		return false;
	}

	// Test helper only: relaxes "same point / mass cap / already engaged", but still goes
	// through the normal pairing so the CP-2 scenarios stay reproducible.
	CancelPairing(ESlimeFusionCancelReason::Rejected);
	OtherFusion->CancelPairing(ESlimeFusionCancelReason::Rejected);

	const FVector Midpoint = (Owner->GetActorLocation() + Other->GetActorLocation()) * 0.5f;
	SetupLocalPairing(Other, Midpoint, /*bInitiator=*/true);
	OtherFusion->SetupLocalPairing(Owner, Midpoint, /*bInitiator=*/false);

	return true;
}

void USlimeFusionComponent::ClearPairData()
{
	Partner = nullptr;
	Participants.Reset();
	ContactTime = 0.f;
	ApproachTime = 0.f;
	ClosestGap = TNumericLimits<float>::Max();
	bIsInitiator = false;

	if (ASlimeEnemyBase* Owner = GetOwnerEnemy())
	{
		Owner->SetFusionTarget(nullptr);
	}
}

void USlimeFusionComponent::StartCooldown(ESlimeFusionCancelReason Reason, float Delay)
{
	if (Delay <= 0.f)
	{
		ClearPairData();
		SetState(ESlimeFusionState::Idle, Reason);
		return;
	}

	CooldownRemaining = Delay;
	SetState(ESlimeFusionState::Cooling, Reason);
}

void USlimeFusionComponent::CancelPairing(ESlimeFusionCancelReason Reason)
{
	if (State == ESlimeFusionState::Idle)
	{
		return;
	}

	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	ASlimeEnemyBase* Other = Partner.Get();

	const USlimeRunConfig* Config = GetRunConfig();
	// Losing the partner needs no retry delay (there is nobody to retry with); everything else
	// waits the configured 1 s, as the design asks ("双方恢复活动，等待1秒后再找对象").
	const bool bImmediate = Reason == ESlimeFusionCancelReason::PartnerLost
		|| Reason == ESlimeFusionCancelReason::Rejected;
	const float Delay = bImmediate ? 0.f : (Config ? Config->FusionRetryDelay : 0.f);

	if (Other && !bResolving)
	{
		if (USlimeFusionComponent* OtherFusion = Other->GetFusionComponent())
		{
			if (OtherFusion != this)
			{
				OtherFusion->SetPairCollisionIgnore(Owner, false);
				OtherFusion->ClearPairData();
				OtherFusion->StartCooldown(Reason, Delay);
			}
		}
	}

	if (Other)
	{
		SetPairCollisionIgnore(Other, false);
	}

	ClearPairData();
	StartCooldown(Reason, Delay);
}

void USlimeFusionComponent::HandlePartnerLost()
{
	CancelPairing(ESlimeFusionCancelReason::PartnerLost);
}

void USlimeFusionComponent::NotifyPartnerDied()
{
	CancelPairing(ESlimeFusionCancelReason::PartnerLost);
}

void USlimeFusionComponent::ResolvePairing()
{
	ASlimeEnemyBase* Survivor = GetOwnerEnemy();
	if (!Survivor || bResolving)
	{
		return;
	}

	// Fold over the participants instead of hard coding "two": sum of mass, of current health
	// and of max health. That is what a future N-way fusion needs and nothing else here changes.
	TArray<ASlimeEnemyBase*> Valid;
	for (const TObjectPtr<ASlimeEnemyBase>& Entry : Participants)
	{
		if (ASlimeEnemyBase* Participant = Entry.Get())
		{
			if (IsValid(Participant) && Participant->GetHealthComponent())
			{
				Valid.AddUnique(Participant);
			}
		}
	}

	if (Valid.Num() < 2)
	{
		HandlePartnerLost();
		return;
	}

	int32 NewMass = 0;
	float SumHealth = 0.f;
	float SumMaxHealth = 0.f;
	for (const ASlimeEnemyBase* Participant : Valid)
	{
		NewMass += Participant->GetMass();

		const USlimeHealthComponent* Health = Participant->GetHealthComponent();
		SumHealth += Health->GetHealth();
		SumMaxHealth += Health->GetMaxHealth();
	}

	// Design 4.4: pooled ratio, then applied to the new mass tier's max health. Undamaged
	// slimes fuse at full health, damaged ones do not heal back up, and the absolute pool may
	// grow - that is the intended "merging is growth" effect.
	const float HealthFraction = SumMaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(SumHealth / SumMaxHealth, 0.f, 1.f)
		: 1.f;

	const USlimeRunConfig* Config = GetRunConfig();
	const float PostFusionDelay = Config ? Config->FusionPostFusionDelay : 0.f;

	// Consume the partners first. bResolving keeps the destroy from looping back into a cancel.
	bResolving = true;
	for (ASlimeEnemyBase* Participant : Valid)
	{
		if (Participant == Survivor)
		{
			continue;
		}

		SetPairCollisionIgnore(Participant, false);
		Participant->SetFusionTarget(nullptr);

		if (USlimeFusionComponent* OtherFusion = Participant->GetFusionComponent())
		{
			OtherFusion->bResolving = true;
			OtherFusion->ClearPairData();
			OtherFusion->SetState(ESlimeFusionState::Idle, ESlimeFusionCancelReason::None);
		}

		Participant->Destroy();
	}
	bResolving = false;

	// The initiator becomes the fused body in place: same actor, same controller, same StateTree.
	Survivor->ApplyStatRow(NewMass, HealthFraction);
	Survivor->SetFusionTarget(nullptr);
	ClearPairData();

	if (USlimeStateComponent* StateComponent = Survivor->GetStateComponent())
	{
		StateComponent->RemoveStateTag(TAG_State_Enemy_Normal_Fusing);
		StateComponent->AddStateTag(TAG_State_Enemy_Normal_Idle);

		const int32 MassCap = GetMassCap();
		if (MassCap > 0 && Survivor->GetMass() >= MassCap)
		{
			// Mass 8 only moves inside its own point and never fuses again (design 4.4).
			StateComponent->AddStateTag(TAG_State_Enemy_Normal_MassLocked);
		}
	}

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("[%s] fused into mass %d (health %.1f / %.1f, fraction %.3f from %d participants)"),
			*GetNameSafe(Survivor), Survivor->GetMass(),
			Survivor->GetHealthComponent() ? Survivor->GetHealthComponent()->GetHealth() : 0.f,
			Survivor->GetHealthComponent() ? Survivor->GetHealthComponent()->GetMaxHealth() : 0.f,
			HealthFraction, Valid.Num());
	}

	// Fusing is not a kill: statistics only, never score (IBattleDirector::OnEnemyFused).
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(Survivor))
	{
		GameMode->OnEnemyFused(Survivor->GetMass());
	}

	StartCooldown(ESlimeFusionCancelReason::None, PostFusionDelay);
}

ESlimeFusionAdvance USlimeFusionComponent::AdvanceApproach(float DeltaTime)
{
	ASlimeEnemyBase* Owner = GetOwnerEnemy();
	if (!Owner)
	{
		return ESlimeFusionAdvance::Finished;
	}

	if (State == ESlimeFusionState::Cooling)
	{
		CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);
		if (CooldownRemaining <= 0.f)
		{
			// Releasing here is what lets the "Hold" state's condition transition fire and send
			// the slime back to wandering.
			const ESlimeFusionCancelReason Reason = LastCancelReason;
			ClearPairData();
			SetState(ESlimeFusionState::Idle, Reason);
			return ESlimeFusionAdvance::Finished;
		}

		return ESlimeFusionAdvance::InProgress;
	}

	if (State != ESlimeFusionState::Approaching && State != ESlimeFusionState::Contacting)
	{
		return ESlimeFusionAdvance::Finished;
	}

	ASlimeEnemyBase* Other = Partner.Get();
	const USlimeHealthComponent* OtherHealth = Other ? Other->GetHealthComponent() : nullptr;
	if (!IsValid(Other) || !OtherHealth || OtherHealth->IsDead())
	{
		HandlePartnerLost();
		return ESlimeFusionAdvance::Finished;
	}

	const USlimeRunConfig* Config = GetRunConfig();
	const int32 MassCap = GetMassCap();
	const bool bConditionsLost =
		Other->GetPointId() != Owner->GetPointId()
		|| Owner->IsAggressive() || Other->IsAggressive()
		|| (MassCap > 0 && Owner->GetMass() + Other->GetMass() > MassCap);

	if (bConditionsLost)
	{
		CancelPairing(ESlimeFusionCancelReason::ConditionsLost);
		return ESlimeFusionAdvance::InProgress;
	}

	const float Distance = FVector::Dist(Owner->GetActorLocation(), Other->GetActorLocation());

	if (Distance <= GetContactDistance(Other))
	{
		if (State != ESlimeFusionState::Contacting)
		{
			SetState(ESlimeFusionState::Contacting, ESlimeFusionCancelReason::None);
		}

		ContactTime += DeltaTime;

		const float RequiredContactTime = Config ? Config->FusionContactTime : 0.f;
		if (RequiredContactTime <= 0.f)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("USlimeFusionComponent: FusionContactTime is 0 in DA_RunConfig; cancelling the fusion."));
			CancelPairing(ESlimeFusionCancelReason::ConditionsLost);
			return ESlimeFusionAdvance::InProgress;
		}

		if (ContactTime >= RequiredContactTime)
		{
			// Only the initiator resolves: one writer, one place where the survivor is decided,
			// and no double resolution when both slimes tick in the same frame.
			if (bIsInitiator)
			{
				ResolvePairing();
			}
			else
			{
				ApproachTime = 0.f;
			}
		}
	}
	else
	{
		if (State == ESlimeFusionState::Contacting || ContactTime > 0.f)
		{
			// Design 4.4: losing contact cancels this attempt (it is not a paused timer).
			CancelPairing(ESlimeFusionCancelReason::Separated);
			return ESlimeFusionAdvance::InProgress;
		}

		// Design 4.6.2 is "连续 2 秒无法继续靠近" = a *no progress* timer, not a total travel
		// budget. Two slimes on opposite sides of the 6 m activity area need more than 2 s of
		// walking even while they get closer the whole way, and treating that as a failure made
		// untroubled pairs cancel over and over.
		const float ProgressEpsilon = GetApproachAcceptanceRadius();
		if (Distance < ClosestGap - ProgressEpsilon)
		{
			ClosestGap = Distance;
			ApproachTime = 0.f;
		}
		else
		{
			ApproachTime += DeltaTime;
		}

		const float ApproachTimeout = Config ? Config->AIApproachTimeout : 0.f;
		if (ApproachTimeout > 0.f && ApproachTime >= ApproachTimeout)
		{
			TimeOutApproach(Distance);
			return ESlimeFusionAdvance::InProgress;
		}
	}

	return ESlimeFusionAdvance::InProgress;
}

void USlimeFusionComponent::TimeOutApproach(float Gap)
{
	if (SlimeCVars::DebugCombatLog != 0)
	{
		const ASlimeEnemyBase* Owner = GetOwnerEnemy();
		const ASlimeEnemyBase* Other = Partner.Get();
		const AAIController* AIController = Owner ? Cast<AAIController>(Owner->GetController()) : nullptr;
		const int32 MoveStatus = AIController ? static_cast<int32>(AIController->GetMoveStatus()) : -1;

		// The gap and the move status are what separate the two possible causes:
		//   gap barely shrinks + move status Idle  -> the path request never moved the pawn
		//                                            (meeting point off the navmesh / blocked)
		//   gap shrinks slowly                     -> just a long walk, raise AIApproachTimeout
		UE_LOG(LogSlimeWar, Log,
			TEXT("[%s] fusion approach made no progress for %.2fs: gap to %s is %.0fcm (contact at %.0fcm, ")
			TEXT("closest %.0fcm, move status %d). Retrying later."),
			*GetNameSafe(Owner), ApproachTime, *GetNameSafe(Other), Gap, GetContactDistance(Other),
			ClosestGap < TNumericLimits<float>::Max() ? ClosestGap : -1.f, MoveStatus);
	}

	CancelPairing(ESlimeFusionCancelReason::ApproachTimeout);
}

void USlimeFusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers the paths that do not go through ASlimeEnemyBase::HandleDeath (SlimeClearEnemies,
	// level teardown, pool despawn): the survivor must not keep a stale partner pointer.
	if (!bResolving && (State == ESlimeFusionState::Approaching || State == ESlimeFusionState::Contacting))
	{
		CancelPairing(ESlimeFusionCancelReason::PartnerLost);
	}

	Super::EndPlay(EndPlayReason);
}
