// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyBrainComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeAlertComponent.h"
#include "Ecology/SlimeCargoPickup.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeFactionSite.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Ecology/SlimeMineralPatch.h"
#include "Ecology/SlimePaintSubsystem.h"
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
        if (Rules)
        {
                GatherRestRemaining = RandomStream.FRandRange(
                        Rules->InitialGatherRestMin,
                        FMath::Max(Rules->InitialGatherRestMin, Rules->InitialGatherRestMax));
        }

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

void USlimeEcologyBrainComponent::ClearResourceMemory()
{
	MineralTarget = nullptr;
	CargoTarget = nullptr;
	ResourceMemoryRemaining = 0.f;
        ResourceTargetHoldRemaining = 0.f;
	GatherTimeRemaining = 0.f;
}

void USlimeEcologyBrainComponent::DebugSetGatherRest(float Seconds)
{
        GatherRestRemaining = FMath::Max(0.f, Seconds);
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

        if (const USlimeAlertComponent* Alert = Enemy->GetAlertComponent())
        {
                const bool bAlertNow = Alert->IsAlerted();
                if (bWasAlerted && !bAlertNow && Enemy->GetCargo() <= 0)
                {
                        const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
                        if (Rules)
                        {
                                GatherRestRemaining = FMath::Max(
                                        GatherRestRemaining,
                                        Rules->AlertPostEmptyGatherRest);
                        }
                }
                bWasAlerted = bAlertNow;
        }

        const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
        const USlimeBodyInteractionComponent* Body = Enemy->GetBodyInteraction();
        if (Enemy->IsRunFrozen() || (Health && Health->IsDead()))
	{
		Behavior = ESlimeEcologyBehavior::Paused;
		DesiredSpeed = 0.f;
		if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
		{
			Locomotion->ClearMoveGoal();
		}
		return;
	}

        if (Body && Body->BlocksVoluntaryMovement())
        {
                Behavior = ESlimeEcologyBehavior::Paused;
                DesiredSpeed = 0.f;
                if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
                {
                        Locomotion->SuspendVoluntaryMovement();
                }
                return;
        }

        ResourceMemoryRemaining = FMath::Max(0.f, ResourceMemoryRemaining - DeltaTime);
        GatherRestRemaining = FMath::Max(0.f, GatherRestRemaining - DeltaTime);
        for (auto It = MineralTargetCooldownRemaining.CreateIterator(); It; ++It)
        {
                It.Value() -= DeltaTime;
                if (!It.Key().IsValid() || It.Value() <= 0.f)
                {
                        It.RemoveCurrent();
                }
        }

	if (const USlimeCoreComponent* Core = Enemy->GetCoreComponent())
	{
                const ESlimeCoreCombatPhase CorePhase = Core->GetCombatPhase();
                const bool bActiveAttack =
                        CorePhase == ESlimeCoreCombatPhase::Windup
                        || CorePhase == ESlimeCoreCombatPhase::Pouncing
                        || CorePhase == ESlimeCoreCombatPhase::Recovering;
                if (bActiveAttack)
		{
			Behavior = ESlimeEcologyBehavior::CoreCombat;
			return;
		}
                if (Enemy->GetCargo() <= 0 && Core->ShouldOverrideBrain())
                {
                        Behavior = ESlimeEcologyBehavior::CoreCombat;
                        return;
                }
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
	NearestEnemy.Reset();
	NearestHostileCore.Reset();
	FVector AllyPositionSum = Enemy->GetActorLocation();
	int32 AllyCount = 1;
	float NearestEnemyDistanceSq = TNumericLimits<float>::Max();
	float NearestHostileCoreDistanceSq = TNumericLimits<float>::Max();

	TArray<ASlimeEcologyEnemy*> Candidates;
	if (Ecology)
	{
		Ecology->QueryNeighbors(
			Enemy,
			FMath::Max(Rules->AllyPerceptionRadius, FMath::Max(
				Rules->CoreSenseRadius,
				Rules->CompetitorSenseRadius)),
			Candidates);
	}

	for (ASlimeEcologyEnemy* Candidate : Candidates)
	{
		const USlimeHealthComponent* Health = Candidate ? Candidate->GetHealthComponent() : nullptr;
		if (!Candidate || Candidate == Enemy)
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

		const float DistanceSq =
			FVector::DistSquared2D(Enemy->GetActorLocation(), Candidate->GetActorLocation());
		if (Candidate->GetFaction() == Enemy->GetFaction())
		{
			if (DistanceSq <= FMath::Square(Rules->AllyPerceptionRadius))
			{
				VisibleAllies.Add(Candidate);
				AllyPositionSum += Candidate->GetActorLocation();
				++AllyCount;
			}
			continue;
		}

		if (DistanceSq <= FMath::Square(Rules->CompetitorSenseRadius)
			&& DistanceSq < NearestEnemyDistanceSq)
		{
			NearestEnemyDistanceSq = DistanceSq;
			NearestEnemy = Candidate;
		}

		const USlimeCoreComponent* CandidateCore = Candidate->GetCoreComponent();
		if (CandidateCore && CandidateCore->IsHostileCore()
			&& DistanceSq <= FMath::Square(Rules->CoreSenseRadius)
			&& DistanceSq < NearestHostileCoreDistanceSq)
		{
			NearestHostileCoreDistanceSq = DistanceSq;
			NearestHostileCore = Candidate;
		}
	}

	GroupCenter = AllyCount > 0 ? AllyPositionSum / static_cast<float>(AllyCount) : HomeLocation;

	if (USlimeAlertComponent* Alert = Enemy->GetAlertComponent())
	{
		const ASlimeEcologyEnemy* Rival = NearestEnemy.Get();
		const bool bDisputeCondition = Rival
			&& FVector::DistSquared2D(Enemy->GetActorLocation(), Rival->GetActorLocation())
				<= FMath::Square(Rules->DisputeRange)
			&& (Enemy->GetCargo() > 0
				|| MineralTarget.IsValid()
				|| Rival->GetCargo() > 0
				|| (Rival->GetCoreComponent() && Rival->GetCoreComponent()->IsHostileCore()));
		Alert->SetDisputeCondition(bDisputeCondition, Rules->PerceptionInterval);
	}

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

bool USlimeEcologyBrainComponent::HasLineOfSightToLocation(const FVector& Location) const
{
        const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
        const UWorld* World = GetWorld();
        if (!Enemy || !World)
        {
                return false;
        }

        FCollisionQueryParams QueryParams(
                SCENE_QUERY_STAT(SlimeEcologyLocationSight), false, Enemy);
        FHitResult Hit;
        return !World->LineTraceSingleByChannel(
                Hit,
                Enemy->GetActorLocation(),
                Location,
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

        if (Enemy->GetCargo() > 0 && UpdateCarryingBehavior())
        {
                return;
        }

        const USlimeAlertComponent* Alert = Enemy->GetAlertComponent();
	if (Alert && Alert->IsWarning())
	{
		Behavior = ESlimeEcologyBehavior::AlertWait;
		DesiredSpeed = 0.f;
		Locomotion->ClearMoveGoal();
		return;
	}

	if (const ASlimeEcologyEnemy* HostileCore = NearestHostileCore.Get())
	{
		const float AvoidDistance = Enemy->GetCargo() > 0
			? Rules->HostileCoreAvoidCarrying
			: Rules->HostileCoreAvoidEmpty;
		if (FVector::DistSquared2D(
			Enemy->GetActorLocation(),
			HostileCore->GetActorLocation()) <= FMath::Square(AvoidDistance))
		{
			Behavior = ESlimeEcologyBehavior::AvoidHostileCore;
			DesiredGoal = ChooseThreatAvoidGoal(HostileCore->GetActorLocation());
			DesiredSpeed = Rules->AlertDangerSpeed;
			Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
			return;
		}
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

        const USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent();
        if (Fusion && Fusion->IsEngaged())
        {
                Behavior = ESlimeEcologyBehavior::FusionSeek;
                DesiredGoal = Fusion->GetMeetingPoint();
                DesiredSpeed = Rules->FusionAutoSeekSpeed;
                return;
        }

	if (UpdateResourceBehavior())
	{
		return;
	}

        if (GatherRestRemaining > 0.f && UpdatePaintBehavior())
        {
                return;
        }

	if (Alert && Alert->IsAlerted())
	{
		if (const ASlimeEcologyEnemy* Rival = NearestEnemy.Get())
		{
			Behavior = ESlimeEcologyBehavior::AlertAvoidEnemy;
			DesiredGoal = ChooseThreatAvoidGoal(Rival->GetActorLocation());
			DesiredSpeed = Rules->AlertDangerSpeed;
			Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
			return;
		}

		Behavior = ESlimeEcologyBehavior::AlertRegroup;
		DesiredGoal = VisibleAllies.Num() > 0 ? GroupCenter : HomeLocation;
		DesiredSpeed = Rules->RegroupSpeed;
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
		return;
	}

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

	DesiredSpeed = ResourceMemoryRemaining > 0.f
		? Rules->ExploreSpeedWithResourceMemory
		: Rules->ExploreSpeed;
	if (bHasExploreGoal)
	{
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
	}
}

bool USlimeEcologyBrainComponent::UpdateCarryingBehavior()
{
        ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
        USlimeLocomotionComponent* Locomotion = GetLocomotion();
        const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
        if (!Enemy || !Locomotion || !Rules || Enemy->GetCargo() <= 0)
        {
                return false;
        }

        const bool bWasAvoiding = Behavior == ESlimeEcologyBehavior::AvoidPlayer;
        const bool bShouldAvoidPlayer = bPlayerVisible
                && (bWasAvoiding
                        ? PlayerDistance < Rules->PlayerAvoidExitDistance
                        : PlayerDistance < Rules->PlayerAvoidEnterDistance);
        if (bShouldAvoidPlayer)
        {
                APawn* Player = CachedPlayer.Get();
                FVector Away = Player
                        ? Enemy->GetActorLocation() - Player->GetActorLocation()
                        : FVector::ForwardVector;
                Away.Z = 0.f;
                Away = Away.GetSafeNormal2D();
                Behavior = ESlimeEcologyBehavior::AvoidPlayer;
                DesiredGoal = ChooseAvoidGoal(Away);
                DesiredSpeed = Rules->PlayerAvoidSpeed;
                Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
                return true;
        }

        if (const ASlimeEcologyEnemy* HostileCore = NearestHostileCore.Get())
        {
                if (FVector::DistSquared2D(
                        Enemy->GetActorLocation(),
                        HostileCore->GetActorLocation())
                        <= FMath::Square(Rules->HostileCoreAvoidCarrying))
                {
                        Behavior = ESlimeEcologyBehavior::AvoidHostileCore;
                        DesiredGoal = ChooseThreatAvoidGoal(HostileCore->GetActorLocation());
                        DesiredSpeed = Rules->AlertDangerSpeed;
                        Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
                        return true;
                }
        }

        return UpdateResourceBehavior();
}

bool USlimeEcologyBrainComponent::UpdatePaintBehavior()
{
        ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
        USlimeLocomotionComponent* Locomotion = GetLocomotion();
        const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
        const USlimePaintSubsystem* Paint = GetWorld()
                ? GetWorld()->GetSubsystem<USlimePaintSubsystem>()
                : nullptr;
        if (!Enemy || !Locomotion || !Rules || !Paint || Enemy->GetCargo() > 0)
        {
                return false;
        }

        const FSlimePaintSplat* Best = nullptr;
        float BestDistanceSq = FMath::Square(Rules->PaintSenseRange);
        for (const FSlimePaintSplat& Splat : Paint->GetSplatRecords())
        {
                const float DistanceSq =
                        FVector::DistSquared2D(Enemy->GetActorLocation(), Splat.Location);
                if (DistanceSq < BestDistanceSq && HasLineOfSightToLocation(Splat.Location))
                {
                        BestDistanceSq = DistanceSq;
                        Best = &Splat;
                }
        }

        if (!Best)
        {
                return false;
        }

        Behavior = ESlimeEcologyBehavior::SeekPaint;
        DesiredGoal = Best->Location;
        const float Distance = FMath::Sqrt(BestDistanceSq);
        if (Distance <= Rules->PaintStopRadius)
        {
                DesiredSpeed = 0.f;
                Locomotion->ClearMoveGoal();
        }
        else
        {
                DesiredSpeed = Rules->PaintApproachSpeed;
                Locomotion->SetMoveGoal(
                        DesiredGoal,
                        DesiredSpeed,
                        Rules->PaintStopRadius);
        }
        return true;
}

bool USlimeEcologyBrainComponent::UpdateResourceBehavior()
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	USlimeLocomotionComponent* Locomotion = GetLocomotion();
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Enemy || !Locomotion || !Ecology || !Rules)
	{
		return false;
	}

	if (Enemy->GetCargo() > 0)
	{
		ASlimeFactionSite* Site = Enemy->GetAssignedSite();
		if (!Site)
		{
			return false;
		}

		const float SiteDistance =
			FVector::Dist2D(Enemy->GetActorLocation(), Site->GetDeliveryLocation());
		if (SiteDistance <= FMath::Max(Rules->DeliveryRange, Site->DeliveryRadius))
		{
			if (Ecology->TryDeliver(Enemy))
			{
				Behavior = ESlimeEcologyBehavior::Explore;
				GatherRestRemaining =
					RandomStream.FRandRange(Rules->GatherRestMin, Rules->GatherRestMax);
				MineralTarget = nullptr;
				CargoTarget = nullptr;
				GatherTimeRemaining = 0.f;
				bHasExploreGoal = false;
				return true;
			}
		}

		Behavior = ESlimeEcologyBehavior::ReturnCargo;
		DesiredGoal = Site->GetDeliveryLocation();
		DesiredSpeed = Rules->CargoSpeed;
		Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
		return true;
	}

	if (GatherRestRemaining > 0.f)
	{
		return false;
	}

	if (ASlimeCargoPickup* Pickup = CargoTarget.Get())
	{
		if (Pickup->GetAmount() <= 0)
		{
			CargoTarget = nullptr;
		}
		else if (FVector::Dist2D(Enemy->GetActorLocation(), Pickup->GetActorLocation())
                        <= Rules->PickupActionRange)
		{
			if (Ecology->TryPickupCargo(Enemy, Pickup))
			{
				Behavior = ESlimeEcologyBehavior::ReturnCargo;
				CargoTarget = nullptr;
				return true;
			}
		}
		else
		{
			Behavior = ESlimeEcologyBehavior::RecoverCargo;
			DesiredGoal = Pickup->GetActorLocation();
			DesiredSpeed = Rules->CargoSpeed;
			Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
			return true;
		}
	}

	if (!CargoTarget.IsValid())
	{
		CargoTarget = Ecology->FindNearestCargoPickup(Enemy, Rules->ResourceSenseRange);
		if (ASlimeCargoPickup* Pickup = CargoTarget.Get())
		{
			Behavior = ESlimeEcologyBehavior::RecoverCargo;
			DesiredGoal = Pickup->GetActorLocation();
			DesiredSpeed = Rules->CargoSpeed;
			Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
			return true;
		}
	}

        ResourceTargetHoldRemaining =
                FMath::Max(0.f, ResourceTargetHoldRemaining - Rules->PerceptionInterval);

        ASlimeMineralPatch* Patch = MineralTarget.Get();
        if (Patch && Patch->Stock > 0
                && FVector::DistSquared2D(Locomotion->GetGoal(), Patch->GetActorLocation())
                        <= FMath::Square(25.f)
                && Locomotion->GetPathFailureCount() >= Rules->MaxPathFailures)
        {
                MineralTargetCooldownRemaining.Add(
                        Patch,
                        Rules->ResourceUnreachableRetrySeconds);
                UE_LOG(LogSlimeWar, Warning,
                        TEXT("EcologyResource: %s cannot path to %s; ignoring it for %.1f s."),
                        *GetNameSafe(Enemy),
                        *GetNameSafe(Patch),
                        Rules->ResourceUnreachableRetrySeconds);
                MineralTarget = nullptr;
                ResourceMemoryRemaining = 0.f;
                ResourceTargetHoldRemaining = 0.f;
                GatherTimeRemaining = 0.f;
                Patch = nullptr;
                return false;
        }

        TSet<TWeakObjectPtr<ASlimeMineralPatch>> IgnoredPatches;
        for (const auto& Pair : MineralTargetCooldownRemaining)
        {
                if (Pair.Key.IsValid())
                {
                        IgnoredPatches.Add(Pair.Key);
                }
        }

        const bool bTargetInvalid =
                !Patch || Patch->Stock <= 0 || ResourceMemoryRemaining <= 0.f;
        bool bShouldSelectTarget = bTargetInvalid;
        if (!bTargetInvalid && ResourceTargetHoldRemaining <= 0.f)
        {
                ASlimeMineralPatch* BetterPatch =
                        Ecology->FindBestMineralPatch(
                                Enemy,
                                Rules->ResourceSenseRange,
                                &IgnoredPatches);
                if (!BetterPatch)
                {
                        BetterPatch =
                                Ecology->FindBestMineralPatch(
                                        Enemy,
                                        Rules->ResourceExplorationRange,
                                        &IgnoredPatches);
                }
                if (BetterPatch && BetterPatch != Patch)
                {
                        Patch = BetterPatch;
                        MineralTarget = Patch;
                        ResourceMemoryRemaining = Rules->ResourceMemory;
                        GatherTimeRemaining = 0.f;
                }
                ResourceTargetHoldRemaining = Rules->ResourceTargetHold;
        }

        if (bShouldSelectTarget)
	{
		Patch = Ecology->FindBestMineralPatch(
                        Enemy,
                        Rules->ResourceSenseRange,
                        &IgnoredPatches);
		if (!Patch)
		{
			Patch = Ecology->FindBestMineralPatch(
                                Enemy,
                                Rules->ResourceExplorationRange,
                                &IgnoredPatches);
		}
		MineralTarget = Patch;
		ResourceMemoryRemaining = Patch ? Rules->ResourceMemory : 0.f;
                ResourceTargetHoldRemaining = Patch ? Rules->ResourceTargetHold : 0.f;
		GatherTimeRemaining = 0.f;
	}

	if (Patch)
	{
		const float Distance =
			FVector::Dist2D(Enemy->GetActorLocation(), Patch->GetActorLocation());
		if (Distance > Rules->GatherRange)
		{
			GatherTimeRemaining = 0.f;
			Behavior = ESlimeEcologyBehavior::Gather;
			DesiredGoal = Patch->GetActorLocation();
			DesiredSpeed = Rules->CargoSpeed;
			Locomotion->SetMoveGoal(DesiredGoal, DesiredSpeed);
			return true;
		}

		Behavior = ESlimeEcologyBehavior::Gather;
		Locomotion->ClearMoveGoal();
		GatherTimeRemaining += Rules->PerceptionInterval;
		if (GatherTimeRemaining >= Rules->GatherTime)
		{
			GatherTimeRemaining = 0.f;
			if (Ecology->TryGather(Enemy, Patch))
			{
				Behavior = ESlimeEcologyBehavior::ReturnCargo;
				MineralTarget = nullptr;
				ResourceMemoryRemaining = 0.f;
                                ResourceTargetHoldRemaining = 0.f;
			}
		}
		return true;
	}

        if (Patch)
        {
                UE_LOG(LogSlimeWar, Verbose,
                        TEXT("EcologyResource: %s selected %s at %.0f cm."),
                        *GetNameSafe(Enemy),
                        *GetNameSafe(Patch),
                        FVector::Dist2D(Enemy->GetActorLocation(), Patch->GetActorLocation()));
        }
        else
        {
                UE_LOG(LogSlimeWar, Verbose,
                        TEXT("EcologyResource: %s has no reachable mineral target; exploring."),
                        *GetNameSafe(Enemy));
        }

	return false;
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
        FVector BestCandidate = GroupCenter;
        bool bHasBestCandidate = false;
        float BestScore = -TNumericLimits<float>::Max();

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

                const float CellSize = FMath::Max(100.f, Rules->ExplorationHistoryCellSize);
                const FIntPoint Cell(
                        FMath::FloorToInt(Candidate.X / CellSize),
                        FMath::FloorToInt(Candidate.Y / CellSize));
                const int32 Visits = ExplorationVisits.FindRef(Cell);
                const float Score =
                        FVector::Dist2D(Candidate, Enemy->GetActorLocation())
                        - Visits * Rules->ExplorationVisitPenalty * 100.f;
                if (Score > BestScore)
                {
                        BestScore = Score;
                        BestCandidate = Candidate;
                        bHasBestCandidate = true;
                }
	}

        DesiredGoal = bHasBestCandidate ? BestCandidate : GroupCenter;
	bHasExploreGoal = true;
	ExploreGoalAge = 0.f;
        if (bHasBestCandidate)
        {
                const float CellSize = FMath::Max(100.f, Rules->ExplorationHistoryCellSize);
                const FIntPoint Cell(
                        FMath::FloorToInt(DesiredGoal.X / CellSize),
                        FMath::FloorToInt(DesiredGoal.Y / CellSize));
                ++ExplorationVisits.FindOrAdd(Cell);
        }
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

FVector USlimeEcologyBrainComponent::ChooseThreatAvoidGoal(const FVector& ThreatLocation) const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Enemy || !Rules)
	{
		return FVector::ZeroVector;
	}

	FVector Away = Enemy->GetActorLocation() - ThreatLocation;
	Away.Z = 0.f;
	Away = Away.GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = Enemy->GetActorForwardVector().GetSafeNormal2D();
	}

	const int32 CandidateCount = FMath::Max(1, Rules->AvoidCandidateCount);
	const float StepAngle = PI / 4.f;
	const FVector Current = Enemy->GetActorLocation();
	FVector Best = Current + Away * Rules->AvoidCandidateDistance;
	float BestScore = -TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < CandidateCount; ++Index)
	{
		const float OffsetIndex =
			static_cast<float>(Index) - (CandidateCount - 1) * 0.5f;
		FVector Direction =
			Away.RotateAngleAxis(OffsetIndex * StepAngle, FVector::UpVector);
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
			FVector::DistSquared2D(Projected, ThreatLocation)
			- Crowding * FMath::Square(100.f);
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
	const USlimeEcologyFusionComponent* Fusion =
		Enemy ? Enemy->GetEcologyFusionComponent() : nullptr;
	const ASlimeEcologyEnemy* Partner = Fusion ? Fusion->GetPartner() : nullptr;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAlly : VisibleAllies)
	{
		const ASlimeEcologyEnemy* Ally = WeakAlly.Get();
		const UCapsuleComponent* AllyCapsule = Ally ? Ally->GetCapsuleComponent() : nullptr;
		if (!Ally || !AllyCapsule || Ally == Partner)
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
        if (Behavior == ESlimeEcologyBehavior::SeekPaint)
        {
                return TEXT("SeekPaint");
        }
	switch (Behavior)
	{
	case ESlimeEcologyBehavior::Paused:			return TEXT("Paused");
	case ESlimeEcologyBehavior::CoreCombat:		return TEXT("CoreCombat");
	case ESlimeEcologyBehavior::AlertWait:		return TEXT("AlertWait");
	case ESlimeEcologyBehavior::AvoidHostileCore:	return TEXT("AvoidHostileCore");
	case ESlimeEcologyBehavior::AlertAvoidEnemy:	return TEXT("AlertAvoidEnemy");
	case ESlimeEcologyBehavior::AlertRegroup:	return TEXT("AlertRegroup");
	case ESlimeEcologyBehavior::AvoidPlayer:	return TEXT("AvoidPlayer");
	case ESlimeEcologyBehavior::Regroup:		return TEXT("Regroup");
	case ESlimeEcologyBehavior::Explore:		return TEXT("Explore");
	case ESlimeEcologyBehavior::FusionSeek:	return TEXT("FusionSeek");
	case ESlimeEcologyBehavior::RecoverCargo:	return TEXT("RecoverCargo");
	case ESlimeEcologyBehavior::ReturnCargo:	return TEXT("ReturnCargo");
	case ESlimeEcologyBehavior::Gather:		return TEXT("Gather");
	default:									return TEXT("Unknown");
	}
}
