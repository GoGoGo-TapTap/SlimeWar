// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeCoreComponent.h"

#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "DrawDebugHelpers.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Ecology/SlimePaintSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SlimeWarCharacter.h"

USlimeCoreComponent::USlimeCoreComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USlimeCoreComponent::BeginPlay()
{
	Super::BeginPlay();
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	HomeLocation = Enemy ? Enemy->GetActivityCenter() : FVector::ZeroVector;
	if (HomeLocation.IsNearlyZero() && Enemy)
	{
		HomeLocation = Enemy->GetActorLocation();
	}
}

ASlimeEcologyEnemy* USlimeCoreComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEcologyEnemy>(GetOwner());
}

USlimeLocomotionComponent* USlimeCoreComponent::GetLocomotion() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetLocomotion() : nullptr;
}

USlimeBodyInteractionComponent* USlimeCoreComponent::GetBody() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	return Enemy ? Enemy->GetBodyInteraction() : nullptr;
}

USlimeEcologySubsystem* USlimeCoreComponent::GetEcologySubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USlimeEcologySubsystem>() : nullptr;
}

const USlimeEcologyRules* USlimeCoreComponent::GetRules() const
{
	return USlimeEcologyRules::GetFor(this);
}

void USlimeCoreComponent::ArmCore(ESlimeCoreState InState)
{
	if (const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy())
	{
		HomeLocation = Enemy->GetActivityCenter();
		if (HomeLocation.IsNearlyZero())
		{
			HomeLocation = Enemy->GetActorLocation();
		}
	}

	const ESlimeCoreState Previous = CoreState;
	CoreState = InState;
	CombatTarget.Reset();
	TargetLastSeenLocation = FVector::ZeroVector;
	CombatMemoryRemaining = 0.f;
	AttackCooldownRemaining = 0.f;
	WindupRemaining = 0.f;
	PounceRemaining = 0.f;
	RecoveryRemaining = 0.f;
	RetreatRemaining = 0.f;
	bExploded = false;

	if (CoreState == ESlimeCoreState::Hostile)
	{
		const USlimeEcologyRules* Rules = GetRules();
		WakeRemaining = Rules ? Rules->CoreWakeTime : 1.2f;
		Phase = ESlimeCoreCombatPhase::Waking;
		if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
		{
			Locomotion->ClearMoveGoal();
		}
	}
	else
	{
		WakeRemaining = 0.f;
		Phase = ESlimeCoreCombatPhase::Idle;
	}

	if (Previous != CoreState)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("Core: %s state -> %s."),
			*GetNameSafe(GetOwner()),
			CoreState == ESlimeCoreState::Hostile ? TEXT("Hostile")
				: CoreState == ESlimeCoreState::Neutral ? TEXT("Neutral") : TEXT("None"));
	}
}

void USlimeCoreComponent::SetPlayerProvoked(float Seconds)
{
	const USlimeEcologyRules* Rules = GetRules();
	PlayerProvokedRemaining = FMath::Max(
		PlayerProvokedRemaining,
		Seconds > 0.f ? Seconds : (Rules ? Rules->CorePlayerProvokedDuration : 6.f));
}

void USlimeCoreComponent::ClearCombat()
{
	CombatTarget.Reset();
	TargetLastSeenLocation = FVector::ZeroVector;
	CombatMemoryRemaining = 0.f;
	Phase = ESlimeCoreCombatPhase::Idle;
}

bool USlimeCoreComponent::ShouldOverrideBrain() const
{
	return CoreState == ESlimeCoreState::Hostile
		&& Phase != ESlimeCoreCombatPhase::Idle;
}

const TCHAR* USlimeCoreComponent::GetPhaseName() const
{
	switch (Phase)
	{
	case ESlimeCoreCombatPhase::Waking:		return TEXT("Waking");
	case ESlimeCoreCombatPhase::Chasing:	return TEXT("Chasing");
	case ESlimeCoreCombatPhase::Windup:		return TEXT("Windup");
	case ESlimeCoreCombatPhase::Pouncing:	return TEXT("Pouncing");
	case ESlimeCoreCombatPhase::Recovering:	return TEXT("Recovering");
	case ESlimeCoreCombatPhase::Retreating:	return TEXT("Retreating");
	default:								return TEXT("Idle");
	}
}

void USlimeCoreComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
	const USlimeEcologyRules* Rules = GetRules();
	if (!Enemy || !Health || !Rules || CoreState != ESlimeCoreState::Hostile)
	{
		return;
	}

	PlayerProvokedRemaining = FMath::Max(0.f, PlayerProvokedRemaining - DeltaTime);
	AttackCooldownRemaining = FMath::Max(0.f, AttackCooldownRemaining - DeltaTime);

	if (Enemy->IsRunFrozen())
	{
		Phase = ESlimeCoreCombatPhase::Idle;
		CombatTarget.Reset();
		return;
	}

	if (const USlimeBodyInteractionComponent* Body = GetBody())
	{
		if (Body->BlocksVoluntaryMovement())
		{
			if (Phase == ESlimeCoreCombatPhase::Windup
				|| Phase == ESlimeCoreCombatPhase::Pouncing)
			{
				Phase = ESlimeCoreCombatPhase::Recovering;
				RecoveryRemaining = Rules->CoreRecovery;
			}
			return;
		}
	}

	if (Phase == ESlimeCoreCombatPhase::Waking)
	{
		WakeRemaining = FMath::Max(0.f, WakeRemaining - DeltaTime);
		if (WakeRemaining <= 0.f)
		{
			Phase = ESlimeCoreCombatPhase::Idle;
		}
		return;
	}

	if (Phase == ESlimeCoreCombatPhase::Windup)
	{
		WindupRemaining -= DeltaTime;
		if (WindupRemaining <= 0.f)
		{
			StartPounce();
		}
		return;
	}

	if (Phase == ESlimeCoreCombatPhase::Pouncing)
	{
		TickPounce(DeltaTime);
		return;
	}

	if (Phase == ESlimeCoreCombatPhase::Recovering)
	{
		RecoveryRemaining -= DeltaTime;
		if (RecoveryRemaining <= 0.f)
		{
			Phase = ESlimeCoreCombatPhase::Idle;
		}
		return;
	}

	if (Phase == ESlimeCoreCombatPhase::Retreating)
	{
		RetreatRemaining -= DeltaTime;
		if (RetreatRemaining <= 0.f)
		{
			Phase = ESlimeCoreCombatPhase::Idle;
		}
		return;
	}

	TargetRefreshRemaining -= DeltaTime;
	if (TargetRefreshRemaining <= 0.f)
	{
		TargetRefreshRemaining = Rules->PerceptionInterval;
		AcquireTarget();
	}

	const float HealthFraction = Health->GetMaxHealth() > 0.f
		? Health->GetHealth() / Health->GetMaxHealth() : 0.f;
	if (HealthFraction < Rules->CoreRetreatHealthFraction && CombatTarget.IsValid())
	{
		CombatTarget.Reset();
		CombatMemoryRemaining = 0.f;
		Phase = ESlimeCoreCombatPhase::Retreating;
		RetreatRemaining = Rules->CoreRetreatDuration;
		if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
		{
			Locomotion->SetMoveGoal(HomeLocation, Rules->CoreChaseSpeed);
		}
		return;
	}

	AActor* Target = CombatTarget.Get();
	if (!Target || CombatMemoryRemaining <= 0.f)
	{
		Phase = ESlimeCoreCombatPhase::Idle;
		return;
	}

	if (FVector::Dist2D(Enemy->GetActorLocation(), HomeLocation) > Rules->CoreLeashDistance)
	{
		CombatTarget.Reset();
		CombatMemoryRemaining = 0.f;
		Phase = ESlimeCoreCombatPhase::Idle;
		return;
	}

	const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());
	const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	const float PounceRange = FMath::Max(
		Rules->PounceBaseRange,
		(Capsule ? Capsule->GetScaledCapsuleRadius() : 20.f) + 65.f);
	if (AttackCooldownRemaining <= 0.f
		&& Distance <= PounceRange
		&& HasLineOfSight(Target))
	{
		StartWindup(Target);
		return;
	}

	Phase = ESlimeCoreCombatPhase::Chasing;
	if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
	{
		const FVector Goal = HasLineOfSight(Target)
			? Target->GetActorLocation()
			: TargetLastSeenLocation;
		Locomotion->SetMoveGoal(Goal, Rules->CoreChaseSpeed);
	}
}

void USlimeCoreComponent::AcquireTarget()
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Enemy || !Rules)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player)
	{
		const float PlayerDistance = FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation());
		if ((PlayerDistance <= Rules->CorePlayerProximity || PlayerProvokedRemaining > 0.f)
			&& HasLineOfSight(Player))
		{
			CombatTarget = Player;
			TargetLastSeenLocation = Player->GetActorLocation();
			CombatMemoryRemaining = Rules->CoreCombatMemory;
			return;
		}
	}

	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Ecology)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Enemy, Rules->CoreSenseRadius, Neighbours);
	ASlimeEcologyEnemy* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	for (ASlimeEcologyEnemy* Candidate : Neighbours)
	{
		const USlimeHealthComponent* CandidateHealth =
			Candidate ? Candidate->GetHealthComponent() : nullptr;
		if (!Candidate || Candidate->GetFaction() == Enemy->GetFaction()
			|| (CandidateHealth && CandidateHealth->IsDead())
			|| !HasLineOfSight(Candidate))
		{
			continue;
		}

		const float DistanceSq =
			FVector::DistSquared2D(Enemy->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	if (Best)
	{
		CombatTarget = Best;
		TargetLastSeenLocation = Best->GetActorLocation();
		CombatMemoryRemaining = Rules->CoreCombatMemory;
		Phase = ESlimeCoreCombatPhase::Chasing;
	}
	else if (CombatTarget.IsValid())
	{
		CombatMemoryRemaining -= Rules->PerceptionInterval;
	}
	else
	{
		CombatMemoryRemaining = 0.f;
		Phase = ESlimeCoreCombatPhase::Idle;
	}
}

bool USlimeCoreComponent::IsValidCombatTarget(const AActor* Target) const
{
	if (!Target)
	{
		return false;
	}
	const USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>();
	return !Health || !Health->IsDead();
}

bool USlimeCoreComponent::HasLineOfSight(const AActor* Target) const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !Target || !World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeCoreSight), false, Owner);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		Owner->GetActorLocation(),
		Target->GetActorLocation(),
		ECC_WorldStatic,
		Params);
}

void USlimeCoreComponent::StartWindup(AActor* Target)
{
	const USlimeEcologyRules* Rules = GetRules();
	if (!Target || !Rules)
	{
		return;
	}

	CombatTarget = Target;
	TargetLastSeenLocation = Target->GetActorLocation();
	FVector Direction = TargetLastSeenLocation - GetOwner()->GetActorLocation();
	Direction.Z = 0.f;
	PounceDirection = Direction.GetSafeNormal2D();
	WindupRemaining = Rules->CoreWindup;
	Phase = ESlimeCoreCombatPhase::Windup;
	if (USlimeLocomotionComponent* Locomotion = GetLocomotion())
	{
		Locomotion->ClearMoveGoal();
	}
	UE_LOG(LogSlimeWar, Log, TEXT("CorePounce: %s windup %.2f s."),
		*GetNameSafe(GetOwner()), Rules->CoreWindup);
}

void USlimeCoreComponent::StartPounce()
{
	const USlimeEcologyRules* Rules = GetRules();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Rules || !Character)
	{
		return;
	}

	PounceHitTargets.Reset();
	PreviousPounceLocation = Character->GetActorLocation();
	PounceRemaining = Rules->PounceDuration;
	Phase = ESlimeCoreCombatPhase::Pouncing;
	Character->LaunchCharacter(
		FVector(PounceDirection.X * Rules->PounceSpeed,
			PounceDirection.Y * Rules->PounceSpeed,
			Rules->PounceLift),
		true,
		true);
}

void USlimeCoreComponent::TickPounce(float DeltaTime)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const FVector Current = Character->GetActorLocation();
	ProcessPounceHits(PreviousPounceLocation, Current);
	PreviousPounceLocation = Current;

	PounceRemaining -= DeltaTime;
	if (PounceRemaining <= 0.f)
	{
		FinishPounce();
	}
}

void USlimeCoreComponent::ProcessPounceHits(const FVector& From, const FVector& To)
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	if (!Enemy || !Rules)
	{
		return;
	}

	const UCapsuleComponent* OwnCapsule = Enemy->GetCapsuleComponent();
	const float OwnRadius = OwnCapsule ? OwnCapsule->GetScaledCapsuleRadius() : 20.f;

	if (AActor* Target = CombatTarget.Get())
	{
		const FVector TargetLocation = Target->GetActorLocation();
		const float Distance = FMath::PointDistToSegment(TargetLocation, From, To);
		if (!PounceHitTargets.Contains(Target)
			&& Distance <= OwnRadius + 60.f
			&& FMath::Abs(TargetLocation.Z - Enemy->GetActorLocation().Z) <= Rules->PounceHitHeight)
		{
			PounceHitTargets.Add(Target);
			ApplyPounceDamage(Target);
		}
	}

	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	if (!Ecology)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Enemy, 500.f, Neighbours);
	for (ASlimeEcologyEnemy* Candidate : Neighbours)
	{
		const UCapsuleComponent* CandidateCapsule =
			Candidate ? Candidate->GetCapsuleComponent() : nullptr;
		if (!Candidate || Candidate->GetFaction() == Enemy->GetFaction()
			|| PounceHitTargets.Contains(Candidate)
			|| !CandidateCapsule)
		{
			continue;
		}

		const float Distance =
			FMath::PointDistToSegment(Candidate->GetActorLocation(), From, To);
		if (Distance <= OwnRadius + CandidateCapsule->GetScaledCapsuleRadius() + 20.f
			&& FMath::Abs(Candidate->GetActorLocation().Z - Enemy->GetActorLocation().Z)
				<= Rules->PounceHitHeight)
		{
			PounceHitTargets.Add(Candidate);
			ApplyPounceDamage(Candidate);
		}
	}
}

void USlimeCoreComponent::ApplyPounceDamage(AActor* Target)
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this);
	if (!Enemy || !Rules || !Combat || !Target)
	{
		return;
	}

	if (ASlimeWarCharacter* Player = Cast<ASlimeWarCharacter>(Target))
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("CorePounceHit: %s -> player %.1f damage."),
			*GetNameSafe(Enemy),
			Rules->CoreDamagePlayer);
		Combat->ApplyDamageTo(Player, Rules->CoreDamagePlayer, Enemy);
		return;
	}

	if (ASlimeEcologyEnemy* Rival = Cast<ASlimeEcologyEnemy>(Target))
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("CorePounceHit: %s -> rival %s %.1f damage (devour check)."),
			*GetNameSafe(Enemy),
			*GetNameSafe(Rival),
			Rules->CoreDamageRival);
		USlimeHealthComponent* RivalHealth = Rival->GetHealthComponent();
		if (IsHostileCore() && RivalHealth && RivalHealth->GetHealth() <= Rules->CoreDamageRival)
		{
			Devour(Rival);
		}
		else
		{
			Combat->ApplyDamageTo(Rival, Rules->CoreDamageRival, Enemy);
		}
	}
}

void USlimeCoreComponent::Devour(ASlimeEcologyEnemy* Victim)
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
	if (!Enemy || !Victim || !Rules || !Health)
	{
		return;
	}

	const int32 Absorbed = FMath::Clamp(
		Victim->GetMass(),
		0,
		Rules->FusionTotalMassCap - Enemy->GetMass());
	if (Absorbed <= 0)
	{
		return;
	}

	const int32 NewMass = Enemy->GetMass() + Absorbed;
	const int32 OldCredit = Enemy->GetBodyScoreCredit();
	const int32 NewCredit = OldCredit + FMath::FloorToInt(
		Victim->GetBodyScoreCredit()
		* static_cast<float>(Absorbed)
		/ FMath::Max(1, Victim->GetMass()));

	float NewMaxHealth = NewMass * 30.f;
	UWorld* World = GetWorld();
	if (const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr)
	{
		if (const UStatTableProvider* Provider = GameInstance->GetSubsystem<UStatTableProvider>())
		{
			FSlimeStatRow Row;
			if (Provider->GetSlimeStat(NewMass, Row))
			{
				NewMaxHealth = Row.MaxHealth;
			}
		}
	}

	const float DesiredHealth =
		Health->GetHealth() + Rules->DevourHealPerMass * Absorbed;
	const float HealthFraction = NewMaxHealth > 0.f
		? FMath::Clamp(DesiredHealth / NewMaxHealth, 0.f, 1.f)
		: 1.f;
	const int32 OwnerCargo = Enemy->GetCargo();
	const int32 VictimCargo = Victim->GetCargo();
	const int32 VictimMass = Victim->GetMass();
	const FVector VictimLocation = Victim->GetActorLocation();

	Enemy->ApplyStatRow(NewMass, HealthFraction);
	Enemy->SetEcologyTotals(OwnerCargo, NewCredit);
	UE_LOG(LogSlimeWar, Log,
		TEXT("CoreDevour: %s + %s -> mass %d, hp %.1f/%.1f, credit %d, victimCargoPendingDrop %d."),
		*GetNameSafe(Enemy),
		*GetNameSafe(Victim),
		NewMass,
		Health->GetHealth(),
		Health->GetMaxHealth(),
		NewCredit,
		VictimCargo);

	if (USlimeEcologySubsystem* Ecology = GetEcologySubsystem())
	{
		if (VictimCargo > 0)
		{
			Ecology->DropCargo(VictimLocation, VictimCargo);
		}
		Ecology->NotifyAgentRemoved(Victim, VictimMass);
	}
	Victim->SetCargo(0);
	Victim->Destroy();
}

void USlimeCoreComponent::FinishPounce()
{
	const USlimeEcologyRules* Rules = GetRules();
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	Phase = ESlimeCoreCombatPhase::Recovering;
	RecoveryRemaining = Rules ? Rules->CoreRecovery : 0.4f;
	AttackCooldownRemaining = Rules ? Rules->CoreAttackInterval : 1.6f;
}

void USlimeCoreComponent::HandleOwnerDeath()
{
	if (CoreState == ESlimeCoreState::None || bExploded)
	{
		return;
	}

	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
	if (!Enemy || !Health || !IsPlayerCauser(Health->GetLastDamageCauser()))
	{
		return;
	}

	Detonate();
}

bool USlimeCoreComponent::IsPlayerCauser(const AActor* Causer) const
{
	return Causer && Causer->IsA<ASlimeWarCharacter>();
}

void USlimeCoreComponent::Detonate()
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = GetRules();
	USlimeEcologySubsystem* Ecology = GetEcologySubsystem();
	const USlimeHealthComponent* Health = Enemy ? Enemy->GetHealthComponent() : nullptr;
	if (!Enemy || !Rules || !Ecology || !Health)
	{
		return;
	}

	bExploded = true;
	AActor* Causer = Health->GetLastDamageCauser();
	const bool bCanChain =
		!Rules->HostileCoresChainOnly || IsHostileCore();
	AActor* ExplosionCauser = bCanChain ? Causer : Enemy;
	const FVector Origin = Enemy->GetActorLocation();
	const float Radius =
		Rules->ExplosionBaseRadius + Rules->ExplosionRadiusPerMass * Enemy->GetMass();
	UE_LOG(LogSlimeWar, Log,
		TEXT("CoreExplosion: %s mass %d radius %.0f caused by %s."),
		*GetNameSafe(Enemy),
		Enemy->GetMass(),
		Radius,
		*GetNameSafe(Causer));
	if (USlimePaintSubsystem* Paint = GetWorld()->GetSubsystem<USlimePaintSubsystem>())
	{
		Paint->RegisterSplat(
			Origin,
			Paint->GetFactionColor(Enemy->GetFaction()),
			Radius * 0.65f,
			FMath::FRandRange(0.f, 2.f * PI));
	}
        DrawDebugSphere(
                GetWorld(),
                Origin,
                Radius,
                24,
                FColor::Orange,
                false,
                0.8f,
                0,
                3.f);

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Enemy, Radius, Neighbours);
	for (ASlimeEcologyEnemy* Other : Neighbours)
	{
		USlimeHealthComponent* OtherHealth = Other ? Other->GetHealthComponent() : nullptr;
		USlimeCoreComponent* OtherCore = Other ? Other->GetCoreComponent() : nullptr;
		if (!Other || !OtherHealth || OtherHealth->IsDead())
		{
			continue;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeCoreExplosionLOS), false, Enemy);
		Params.AddIgnoredActor(Other);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(
			Hit,
			Origin,
			Other->GetActorLocation(),
			ECC_WorldStatic,
			Params))
		{
                        UE_LOG(LogSlimeWar, Log,
                                TEXT("CoreExplosionEffect: %s blocked by wall."),
                                *GetNameSafe(Other));
			continue;
		}

			if (bCanChain
				&& OtherCore && OtherCore->IsCore() && !OtherCore->HasExploded())
		{
                        DrawDebugLine(
                                GetWorld(),
                                Origin,
                                Other->GetActorLocation(),
                                FColor::Red,
                                false,
                                0.8f,
                                0,
                                4.f);
                        UE_LOG(LogSlimeWar, Log,
                                TEXT("CoreExplosionEffect: chain detonate %s."),
                                *GetNameSafe(Other));
			if (Causer && Causer->IsA<ASlimeWarCharacter>())
			{
				Other->MarkChainKill();
			}
			OtherHealth->ApplyDamage(OtherHealth->GetHealth(), Causer);
			continue;
		}

		const float Distance = FVector::Dist2D(Origin, Other->GetActorLocation());
		const float Alpha = FMath::Clamp(Distance / FMath::Max(1.f, Radius), 0.f, 1.f);
		const float Damage = Rules->ExplosionDamage
			* FMath::Lerp(1.f, Rules->ExplosionEdgeScale, Alpha);
		if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this))
		{
                        const float HealthBefore = OtherHealth->GetHealth();
					Combat->ApplyDamageTo(Other, Damage, ExplosionCauser);
                        UE_LOG(LogSlimeWar, Log,
                                TEXT("CoreExplosionEffect: %s damage %.1f hp %.1f -> %.1f."),
                                *GetNameSafe(Other),
                                Damage,
                                HealthBefore,
                                OtherHealth->GetHealth());
		}

		if (USlimeBodyInteractionComponent* Body = Other->GetBodyInteraction())
		{
			FVector Push = Other->GetActorLocation() - Origin;
			Push.Z = 0.f;
			Push = Push.GetSafeNormal2D();
			const float MassScale = FMath::Pow(
				static_cast<float>(FMath::Max(1, Other->GetMass())),
				Rules->ExplosionPushMassExponent);
                        const float PushSpeed = Rules->ExplosionPush / MassScale;
                        Body->ApplyExternalVelocity(Push * PushSpeed, Causer);
                        UE_LOG(LogSlimeWar, Log,
                                TEXT("CoreExplosionEffect: %s pushed %.1f cm/s."),
                                *GetNameSafe(Other),
                                PushSpeed);
		}

                DrawDebugLine(
                        GetWorld(),
                        Origin,
                        Other->GetActorLocation(),
                        FColor::Yellow,
                        false,
                        0.8f,
                        0,
                        3.f);
                DrawDebugSphere(
                        GetWorld(),
                        Other->GetActorLocation(),
                        45.f,
                        8,
                        FColor::Red,
                        false,
                        0.8f,
                        0,
                        2.f);
	}
}

void USlimeCoreComponent::ForceTargetForTest(AActor* InTarget)
{
	CombatTarget = InTarget;
	TargetLastSeenLocation = InTarget ? InTarget->GetActorLocation() : FVector::ZeroVector;
	CombatMemoryRemaining = InTarget ? 999.f : 0.f;
	WakeRemaining = 0.f;
	Phase = InTarget ? ESlimeCoreCombatPhase::Chasing : ESlimeCoreCombatPhase::Idle;
}

bool USlimeCoreComponent::DebugDevour(ASlimeEcologyEnemy* Victim)
{
	if (!IsHostileCore() || !Victim)
	{
		return false;
	}
	Devour(Victim);
	return true;
}
