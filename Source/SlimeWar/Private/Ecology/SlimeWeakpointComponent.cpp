// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeWeakpointComponent.h"

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologyRules.h"

USlimeWeakpointComponent::USlimeWeakpointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void USlimeWeakpointComponent::ConfigureForMass(int32 Mass, float BodyRadius)
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	bWeakpointActive = Rules && Mass >= Rules->WeakpointStartMass;

	SetCollisionEnabled(bWeakpointActive
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision);
	SetVisibility(bWeakpointActive, true);

	if (!bWeakpointActive || !Rules)
	{
		return;
	}

	SetSphereRadius(Rules->WeakpointRadius, true);
	SetRelativeLocation(FVector(0.f, 0.f, BodyRadius + Rules->WeakpointHeight));
}

bool USlimeWeakpointComponent::TryExecute(AActor* Target, AActor* Causer)
{
	if (!bWeakpointActive || !Target || Target != GetOwner())
	{
		return false;
	}

	USlimeHealthComponent* Health = Target->FindComponentByClass<USlimeHealthComponent>();
	if (!Health || Health->IsDead())
	{
		return true;
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("Weakpoint: %s executed by %s."),
		*GetNameSafe(Target),
		*GetNameSafe(Causer));
	Health->ApplyDamage(Health->GetHealth(), Causer);
	return true;
}
