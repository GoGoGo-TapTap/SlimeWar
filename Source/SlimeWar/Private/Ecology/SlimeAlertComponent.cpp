// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeAlertComponent.h"

#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Engine/World.h"

USlimeAlertComponent::USlimeAlertComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ASlimeEcologyEnemy* USlimeAlertComponent::GetOwnerEnemy() const
{
	return Cast<ASlimeEcologyEnemy>(GetOwner());
}

bool USlimeAlertComponent::CanReceiveAlert() const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeCoreComponent* Core = Enemy ? Enemy->GetCoreComponent() : nullptr;
	const USlimeEcologyFusionComponent* Fusion =
		Enemy ? Enemy->GetEcologyFusionComponent() : nullptr;
	return Enemy
		&& (!Core || Core->GetCoreState() == ESlimeCoreState::None)
		&& (!Fusion
			|| (Fusion->GetBlockRemaining() <= 0.f
				&& Fusion->GetSplitCalmRemaining() <= 0.f));
}

void USlimeAlertComponent::RaiseAlert(AActor* Source, bool bPropagate)
{
	ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Enemy || !Rules || !CanReceiveAlert())
	{
		return;
	}

	if (!IsAlerted())
	{
		WarningRemaining = Rules->AlertWarningTime;
	}
	AlertRemaining = Rules->AlertDuration;
	AlertSource = Source;

	UE_LOG(LogSlimeWar, Verbose,
		TEXT("EcologyAlert: %s raised by %s (warning %.2f, alert %.2f, propagate %s)."),
		*GetNameSafe(Enemy),
		*GetNameSafe(Source),
		WarningRemaining,
		AlertRemaining,
		bPropagate ? TEXT("yes") : TEXT("no"));

	if (!bPropagate)
	{
		return;
	}

	const UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Ecology)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryNeighbors(Enemy, Rules->AlertPropagationRadius, Neighbours);
	for (ASlimeEcologyEnemy* Other : Neighbours)
	{
		if (!Other || Other->GetFaction() != Enemy->GetFaction() || !HasLineOfSight(Other))
		{
			continue;
		}
		if (USlimeAlertComponent* OtherAlert = Other->GetAlertComponent())
		{
			OtherAlert->RaiseAlert(Source, false);
		}
	}
}

void USlimeAlertComponent::NotifyNpcDamage(AActor* Causer)
{
	const ASlimeEcologyEnemy* SourceEnemy = Cast<ASlimeEcologyEnemy>(Causer);
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	if (SourceEnemy && Enemy && SourceEnemy->GetFaction() != Enemy->GetFaction())
	{
		RaiseAlert(Causer, true);
	}
}

void USlimeAlertComponent::SetDisputeCondition(bool bSatisfied, float DeltaTime)
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Rules || DeltaTime <= 0.f)
	{
		return;
	}

	if (bSatisfied && DisputeCooldownRemaining <= 0.f)
	{
		DisputeAccumulator += DeltaTime;
		if (DisputeAccumulator >= Rules->DisputeSeconds)
		{
			DisputeAccumulator = 0.f;
			DisputeCooldownRemaining = Rules->DisputeCooldown;
			RaiseAlert(nullptr, true);
		}
	}
	else
	{
		DisputeAccumulator = FMath::Max(0.f, DisputeAccumulator - DeltaTime);
	}
}

void USlimeAlertComponent::ClearAlert()
{
	WarningRemaining = 0.f;
	AlertRemaining = 0.f;
	AlertSource.Reset();
}

bool USlimeAlertComponent::HasLineOfSight(const AActor* Target) const
{
	const ASlimeEcologyEnemy* Enemy = GetOwnerEnemy();
	const UWorld* World = GetWorld();
	if (!Enemy || !Target || !World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeAlertSight), false, Enemy);
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		Enemy->GetActorLocation(),
		Target->GetActorLocation(),
		ECC_WorldStatic,
		Params);
}

void USlimeAlertComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DisputeCooldownRemaining = FMath::Max(0.f, DisputeCooldownRemaining - DeltaTime);
	if (WarningRemaining > 0.f)
	{
		WarningRemaining = FMath::Max(0.f, WarningRemaining - DeltaTime);
	}
	if (AlertRemaining > 0.f)
	{
		AlertRemaining = FMath::Max(0.f, AlertRemaining - DeltaTime);
	}
	if (WarningRemaining <= 0.f && AlertRemaining <= 0.f)
	{
		AlertSource.Reset();
	}
}
