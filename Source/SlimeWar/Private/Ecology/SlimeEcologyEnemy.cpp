// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyEnemy.h"

#include "Ecology/SlimeEcologyAIController.h"
#include "Ecology/SlimeAlertComponent.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeCargoVisual.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimePaintSubsystem.h"
#include "Ecology/SlimeFactionSite.h"
#include "Ecology/SlimeLocomotionComponent.h"
#include "Ecology/SlimeWeakpointComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "Player/SlimeWarCharacter.h"

ASlimeEcologyEnemy::ASlimeEcologyEnemy()
{
	Mass = 1;
	AIControllerClass = ASlimeEcologyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	Brain = CreateDefaultSubobject<USlimeEcologyBrainComponent>(TEXT("EcologyBrain"));
	Alert = CreateDefaultSubobject<USlimeAlertComponent>(TEXT("Alert"));
	Locomotion = CreateDefaultSubobject<USlimeLocomotionComponent>(TEXT("Locomotion"));
	BodyInteraction = CreateDefaultSubobject<USlimeBodyInteractionComponent>(TEXT("BodyInteraction"));
	EcologyFusion = CreateDefaultSubobject<USlimeEcologyFusionComponent>(TEXT("EcologyFusion"));
	Core = CreateDefaultSubobject<USlimeCoreComponent>(TEXT("Core"));
	Weakpoint = CreateDefaultSubobject<USlimeWeakpointComponent>(TEXT("Weakpoint"));
	Weakpoint->SetupAttachment(GetCapsuleComponent());

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(GetCapsuleComponent());
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetGenerateOverlapEvents(false);
	CoreMesh->SetCanEverAffectNavigation(false);
	CoreMesh->SetReceivesDecals(false);

	SpikeLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spike_Left"));
	SpikeLeft->SetupAttachment(GetCapsuleComponent());
	SpikeLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpikeLeft->SetGenerateOverlapEvents(false);
	SpikeLeft->SetCanEverAffectNavigation(false);
	SpikeLeft->SetReceivesDecals(false);
	SpikeLeft->SetVisibility(false);

	SpikeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spike_Right"));
	SpikeRight->SetupAttachment(GetCapsuleComponent());
	SpikeRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpikeRight->SetGenerateOverlapEvents(false);
	SpikeRight->SetCanEverAffectNavigation(false);
	SpikeRight->SetReceivesDecals(false);
	SpikeRight->SetVisibility(false);

	WeakpointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakpointMesh"));
	WeakpointMesh->SetupAttachment(Weakpoint);
	WeakpointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeakpointMesh->SetGenerateOverlapEvents(false);
	WeakpointMesh->SetCanEverAffectNavigation(false);
	WeakpointMesh->SetReceivesDecals(false);
	WeakpointMesh->SetVisibility(false);
}

void ASlimeEcologyEnemy::BeginPlay()
{
	Super::BeginPlay();

	ApplyFactionVisuals();
	RefreshCoreVisuals();

	if (Brain)
	{
		Brain->SetHome(GetActivityCenter(), GetActivityRadius());
	}
	if (USlimeHealthComponent* HealthComponent = GetHealthComponent())
	{
		HealthComponent->OnDamaged.AddDynamic(this, &ASlimeEcologyEnemy::HandleDamaged);
	}
	RefreshCarriedCargoVisual();

	if (UWorld* World = GetWorld())
	{
		if (USlimeEcologySubsystem* Ecology = World->GetSubsystem<USlimeEcologySubsystem>())
		{
			Ecology->RegisterAgent(this);
		}
	}
	if (USlimeHealthComponent* HealthComponent = GetHealthComponent())
	{
		HealthComponent->OnDamaged.RemoveAll(this);
	}
}

void ASlimeEcologyEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (USlimeEcologySubsystem* Ecology = World->GetSubsystem<USlimeEcologySubsystem>())
		{
			Ecology->UnregisterAgent(this);
		}
	}

	if (Locomotion)
	{
		Locomotion->ClearMoveGoal();
	}
	if (ASlimeCargoVisual* CargoVisual = CarriedCargoVisual.Get())
	{
		CargoVisual->Destroy();
	}
	CarriedCargoVisual.Reset();
	if (EcologyFusion)
	{
		EcologyFusion->NotifyPartnerDied();
	}

	Super::EndPlay(EndPlayReason);
}

void ASlimeEcologyEnemy::InitializeEcology(
	ESlimeFaction InFaction,
	int32 InPointId,
	const FVector& InActivityCenter,
	float InActivityRadius)
{
	SetFaction(InFaction);
	InitializeFromSpawn(InPointId, InActivityCenter, InActivityRadius);
	BodyScoreCredit = FMath::Max(BodyScoreCredit, BodyScoreForMass(GetMass()));

	if (Brain)
	{
		Brain->SetHome(InActivityCenter, InActivityRadius);
		Brain->ForceRefresh();
	}
}

void ASlimeEcologyEnemy::SetFaction(ESlimeFaction InFaction)
{
	Faction = InFaction;
	ApplyFactionVisuals();
	RefreshCoreVisuals();
}

void ASlimeEcologyEnemy::ApplyFactionVisuals()
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (!Rules || !BodyMesh)
	{
		return;
	}

	const USlimeEcologyRules* DefaultRules = GetDefault<USlimeEcologyRules>();
	bool bBodyMeshChanged = false;
	UMaterialInterface* FactionMaterial = nullptr;

	const FSlimeFactionVisual* Visual = Rules->FindFactionVisual(Faction);
	if (!Visual)
	{
		Visual = GetDefault<USlimeEcologyRules>()->FindFactionVisual(Faction);
	}

	if (Visual)
	{
		if (UStaticMesh* LoadedBodyMesh = Visual->BodyMesh.LoadSynchronous())
		{
			if (BodyMesh->GetStaticMesh() != LoadedBodyMesh)
			{
				BodyMesh->SetStaticMesh(LoadedBodyMesh);
				bBodyMeshChanged = true;
			}
		}

		FactionMaterial = Visual->BodyMaterial.LoadSynchronous();
		if (FactionMaterial)
		{
			// SM_Slime_Base has separate eye/body sections. Apply the faction
			// instance to every body section so the white body does not remain.
			for (int32 MaterialIndex = 0; MaterialIndex < BodyMesh->GetNumMaterials(); ++MaterialIndex)
			{
				BodyMesh->SetMaterial(MaterialIndex, FactionMaterial);
			}
		}
	}

	if (SpikeLeft)
	{
		const TSoftObjectPtr<UStaticMesh>& SpikeAsset =
			Rules->SpikeMesh.IsNull() ? DefaultRules->SpikeMesh : Rules->SpikeMesh;
		if (UStaticMesh* LoadedSpikeMesh = SpikeAsset.LoadSynchronous())
		{
			SpikeLeft->SetStaticMesh(LoadedSpikeMesh);
		}
		SpikeLeft->SetMaterial(0, FactionMaterial);
	}

	if (SpikeRight)
	{
		const TSoftObjectPtr<UStaticMesh>& SpikeAsset =
			Rules->SpikeMesh.IsNull() ? DefaultRules->SpikeMesh : Rules->SpikeMesh;
		if (UStaticMesh* LoadedSpikeMesh = SpikeAsset.LoadSynchronous())
		{
			SpikeRight->SetStaticMesh(LoadedSpikeMesh);
		}
		SpikeRight->SetMaterial(0, FactionMaterial);
	}

	if (CoreMesh)
	{
		const TSoftObjectPtr<UStaticMesh>& CoreAsset =
			Rules->CoreMesh.IsNull() ? DefaultRules->CoreMesh : Rules->CoreMesh;
		const TSoftObjectPtr<UMaterialInterface>& CoreMaterialAsset =
			Rules->CoreMaterial.IsNull() ? DefaultRules->CoreMaterial : Rules->CoreMaterial;
		if (UStaticMesh* LoadedCoreMesh = CoreAsset.LoadSynchronous())
		{
			CoreMesh->SetStaticMesh(LoadedCoreMesh);
		}
		CoreMesh->SetMaterial(0, CoreMaterialAsset.LoadSynchronous());
	}

	if (WeakpointMesh)
	{
		const TSoftObjectPtr<UStaticMesh>& WeakpointAsset =
			Rules->WeakpointMesh.IsNull() ? DefaultRules->WeakpointMesh : Rules->WeakpointMesh;
		const TSoftObjectPtr<UMaterialInterface>& WeakpointMaterialAsset =
			Rules->WeakpointMaterial.IsNull() ? DefaultRules->WeakpointMaterial : Rules->WeakpointMaterial;
		if (UStaticMesh* LoadedWeakpointMesh = WeakpointAsset.LoadSynchronous())
		{
			WeakpointMesh->SetStaticMesh(LoadedWeakpointMesh);
		}
		WeakpointMesh->SetMaterial(0, WeakpointMaterialAsset.LoadSynchronous());
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 20.f;
	const float AttachmentScale = Radius / 20.f;

	if (CoreMesh)
	{
		CoreMesh->SetRelativeLocation(FVector(0.f, 0.f, Radius * 0.5f));
		CoreMesh->SetRelativeScale3D(FVector(AttachmentScale));
	}
	if (SpikeLeft)
	{
		SpikeLeft->SetRelativeLocation(FVector(-Radius * 0.7f, 0.f, Radius * 0.3f));
		SpikeLeft->SetRelativeScale3D(FVector(AttachmentScale));
	}
	if (SpikeRight)
	{
		SpikeRight->SetRelativeLocation(FVector(Radius * 0.7f, 0.f, Radius * 0.3f));
		SpikeRight->SetRelativeScale3D(FVector(AttachmentScale));
	}
	if (WeakpointMesh)
	{
		WeakpointMesh->SetRelativeLocation(FVector::ZeroVector);
		WeakpointMesh->SetRelativeScale3D(FVector::OneVector);
		WeakpointMesh->SetVisibility(Weakpoint && Weakpoint->IsVisible(), true);
	}

	if (bBodyMeshChanged && Radius > 0.f)
	{
		ApplyBodyRadius(Radius);
	}
}

void ASlimeEcologyEnemy::RefreshCoreVisuals()
{
	const bool bHostile = Core && Core->IsHostileCore();

	if (CoreMesh)
	{
		CoreMesh->SetVisibility(true, true);
	}
	if (SpikeLeft)
	{
		SpikeLeft->SetVisibility(bHostile, true);
	}
	if (SpikeRight)
	{
		SpikeRight->SetVisibility(bHostile, true);
	}
	if (WeakpointMesh)
	{
		WeakpointMesh->SetVisibility(Weakpoint && Weakpoint->IsVisible(), true);
	}
}

void ASlimeEcologyEnemy::HandleDamaged(AActor* Causer)
{
	NotifyCombatDamage(Causer);
}

void ASlimeEcologyEnemy::NotifyCombatDamage(AActor* Causer)
{
	if (ASlimeWarCharacter* Player = Cast<ASlimeWarCharacter>(Causer))
	{
		if (Core && Core->IsHostileCore())
		{
			Core->SetPlayerProvoked();
		}
		return;
	}

	if (Alert)
	{
		Alert->NotifyNpcDamage(Causer);
	}
}

void ASlimeEcologyEnemy::ApplyAssimilation(ESlimeFaction InFaction)
{
	UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	ASlimeFactionSite* NewSite = Ecology
		? Ecology->GetSiteForFaction(InFaction)
		: nullptr;

	SetFaction(InFaction);
	if (NewSite)
	{
		SetAssignedSite(NewSite);
		InitializeFromSpawn(
			static_cast<int32>(GetTypeHash(NewSite->SiteId) & 0x7fffffff),
			NewSite->GetActorLocation(),
			NewSite->ActivityRadius);
	}
	if (Alert)
	{
		Alert->ClearAlert();
	}
	if (EcologyFusion)
	{
		EcologyFusion->ResetForAssimilation();
	}
	if (Core)
	{
		Core->ClearCombat();
	}
	if (Brain)
	{
		Brain->ClearResourceMemory();
		if (NewSite)
		{
			Brain->SetHome(NewSite->GetActorLocation(), NewSite->ActivityRadius);
		}
		Brain->ForceRefresh();
	}
}

void ASlimeEcologyEnemy::SetAssignedSite(ASlimeFactionSite* InSite)
{
	AssignedSite = InSite;
}

void ASlimeEcologyEnemy::HandleDeath()
{
	if (Core)
	{
		Core->HandleOwnerDeath();
	}
	if (EcologyFusion)
	{
		EcologyFusion->NotifyPartnerDied();
	}

	PendingKillContext = BuildKillContext();
	const bool bPlayerKill = PendingKillContext->bKilledByPlayer;
	if (!bPlayerKill && Cargo > 0)
	{
		if (USlimeEcologySubsystem* Ecology = GetWorld()
			? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
			: nullptr)
		{
			Ecology->DropCargo(GetActorLocation(), Cargo);
		}
	}
	Cargo = 0;
	RefreshCarriedCargoVisual();

	if (USlimeEcologySubsystem* Ecology = GetWorld()
		? GetWorld()->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr)
	{
		Ecology->NotifyAgentRemoved(this, GetMass());
	}

	if (UWorld* World = GetWorld())
	{
		if (USlimePaintSubsystem* Paint = World->GetSubsystem<USlimePaintSubsystem>())
		{
			const float SplatRadius = FMath::Max(
				55.f,
				(GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 20.f)
					* 1.6f);
			Paint->RegisterSplat(
				GetActorLocation(),
				Paint->GetFactionColor(Faction),
				SplatRadius,
				FMath::FRandRange(0.f, 2.f * PI),
				Faction);
		}
	}

	Super::HandleDeath();
}

FSlimeKillContext ASlimeEcologyEnemy::BuildKillContext() const
{
	if (PendingKillContext.IsSet())
	{
		return PendingKillContext.GetValue();
	}

	FSlimeKillContext Context = Super::BuildKillContext();
	Context.Cargo = Cargo;
	Context.BodyScoreCredit = BodyScoreCredit;
	Context.Faction = Faction;
	Context.CoreState = Core ? Core->GetCoreState() : ESlimeCoreState::None;
	Context.bHostileCore = Context.CoreState == ESlimeCoreState::Hostile;
	Context.bDetonated = Core && Core->HasExploded();
	Context.bChainKill = bChainKill;

	const USlimeHealthComponent* HealthComponent = GetHealthComponent();
	AActor* Causer = HealthComponent ? HealthComponent->GetLastDamageCauser() : nullptr;
	Context.Causer = Causer;
	Context.bKilledByPlayer = Causer && Causer->IsA<ASlimeWarCharacter>();
	return Context;
}

void ASlimeEcologyEnemy::OnBodyStatsApplied(int32 NewMass, float BodyRadius)
{
	if (Weakpoint)
	{
		Weakpoint->ConfigureForMass(NewMass, BodyRadius);
	}

	ApplyFactionVisuals();
	RefreshCoreVisuals();
}

void ASlimeEcologyEnemy::SetEcologyTotals(int32 InCargo, int32 InBodyScoreCredit)
{
	Cargo = FMath::Max(0, InCargo);
	BodyScoreCredit = FMath::Max(0, InBodyScoreCredit);
	RefreshCarriedCargoVisual();
}

void ASlimeEcologyEnemy::SetCargo(int32 InCargo)
{
	Cargo = FMath::Max(0, InCargo);
	RefreshCarriedCargoVisual();
}

void ASlimeEcologyEnemy::AddCargo(int32 Delta)
{
	Cargo = FMath::Max(0, Cargo + Delta);
	RefreshCarriedCargoVisual();
}

bool ASlimeEcologyEnemy::ConsumeCargo(int32 Count)
{
	if (Count <= 0 || Cargo < Count)
	{
		return false;
	}

	Cargo -= Count;
	RefreshCarriedCargoVisual();
	return true;
}

void ASlimeEcologyEnemy::RefreshCarriedCargoVisual()
{
	if (IsActorBeingDestroyed())
	{
		return;
	}

	ASlimeCargoVisual* CargoVisual = CarriedCargoVisual.Get();
	if (Cargo <= 0)
	{
		if (CargoVisual)
		{
			CargoVisual->Destroy();
		}
		CarriedCargoVisual.Reset();
		return;
	}

	if (!CargoVisual)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CargoVisual = World->SpawnActor<ASlimeCargoVisual>(
			ASlimeCargoVisual::StaticClass(),
			GetActorLocation(),
			FRotator::ZeroRotator,
			SpawnParameters);
		if (!CargoVisual)
		{
			return;
		}
		CargoVisual->AttachToComponent(
			GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		const float Radius = GetCapsuleComponent()
			? GetCapsuleComponent()->GetScaledCapsuleRadius()
			: 20.f;
		CargoVisual->SetActorRelativeLocation(FVector(0.f, 0.f, Radius * 0.65f));
		CargoVisual->SetActorRelativeRotation(FRotator::ZeroRotator);
		CarriedCargoVisual = CargoVisual;
	}

	CargoVisual->SetVisualAmount(Cargo);
}

int32 ASlimeEcologyEnemy::BodyScoreForMass(int32 InMass)
{
	const int32 SafeMass = FMath::Max(1, InMass);
	const int32 LargeBonus = FMath::Max(0, SafeMass - 2);
	return SafeMass * 4 + LargeBonus * LargeBonus;
}

bool ASlimeEcologyEnemy::TryImpactSplit(
	const FVector& ImpactPoint,
	const FVector& ImpactNormal)
{
	UWorld* World = GetWorld();
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	USlimeHealthComponent* ParentHealth = GetHealthComponent();
	const int32 FragmentCount = GetMass();
	if (bSplitting || !World || !Rules || !ParentHealth || FragmentCount < 2)
	{
		return false;
	}

	FSlimeStatRow FragmentRow;
	UStatTableProvider* Provider = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UStatTableProvider>()
		: nullptr;
	if (!Provider || !Provider->GetSlimeStat(1, FragmentRow) || FragmentRow.MaxHealth <= 0.f)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FVector Origin = GetActorLocation();
	const float ParentRadius =
		GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : FragmentRow.BodyRadius;
	const float FragmentRadius = FragmentRow.BodyRadius;
	FVector ImpactNormal2D = ImpactNormal;
	ImpactNormal2D.Z = 0.f;
	ImpactNormal2D = ImpactNormal2D.GetSafeNormal2D();
	if (ImpactNormal2D.IsNearlyZero())
	{
		ImpactNormal2D = FVector::ForwardVector;
	}
	const FVector RingCentre =
		Origin + ImpactNormal2D * (FragmentRadius + 5.f);
	const float BaseRadius = FMath::Max(ParentRadius + 30.f, FragmentRadius * 3.f);
	const float RingRadii[] = {
		BaseRadius,
		BaseRadius + 45.f,
		BaseRadius + 90.f
	};

	TArray<FVector> SpawnLocations;
	SpawnLocations.Reserve(FragmentCount);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeImpactSplit), false, this);
	const int32 AngleSteps = FMath::Max(16, FragmentCount * 3);

	for (int32 RingIndex = 0; RingIndex < UE_ARRAY_COUNT(RingRadii)
		&& SpawnLocations.Num() < FragmentCount; ++RingIndex)
	{
		const float RingRadius = RingRadii[RingIndex];
		for (int32 Step = 0; Step < AngleSteps
			&& SpawnLocations.Num() < FragmentCount; ++Step)
		{
			const float Angle =
				2.f * PI * (static_cast<float>(Step) + 0.2f * RingIndex)
				/ static_cast<float>(AngleSteps);
			FVector Candidate = RingCentre + FVector(
				FMath::Cos(Angle) * RingRadius,
				FMath::Sin(Angle) * RingRadius,
				0.f);
			Candidate.Z = Origin.Z;

			if (FVector::DotProduct(Candidate - ImpactPoint, ImpactNormal2D)
				< FragmentRadius + 5.f)
			{
				continue;
			}

			FNavLocation Projected;
			if (NavSystem && !NavSystem->ProjectPointToNavigation(
				Candidate,
				Projected,
				FVector(100.f, 100.f, 500.f)))
			{
				continue;
			}
			if (NavSystem)
			{
				Candidate = Projected.Location;
				Candidate.Z = Origin.Z;
			}

			bool bOverlapsPrevious = false;
			for (const FVector& Previous : SpawnLocations)
			{
				if (FVector::Dist2D(Candidate, Previous) < FragmentRadius * 2.f + 5.f)
				{
					bOverlapsPrevious = true;
					break;
				}
			}
			if (bOverlapsPrevious)
			{
				continue;
			}

			if (World->OverlapBlockingTestByChannel(
				Candidate,
				FQuat::Identity,
				ECC_Pawn,
				FCollisionShape::MakeSphere(FragmentRadius * 0.8f),
				QueryParams))
			{
				continue;
			}

			SpawnLocations.Add(Candidate);
		}
	}

	if (SpawnLocations.Num() < FragmentCount)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ImpactSplit: %s rejected, only %d / %d legal fragment positions ")
			TEXT("(impact %s normal %s)."),
			*GetNameSafe(this),
			SpawnLocations.Num(),
			FragmentCount,
			*ImpactPoint.ToCompactString(),
			*ImpactNormal.ToCompactString());
		return false;
	}

	const float ParentHealthValue = ParentHealth->GetHealth();
	const int32 ParentCargo = GetCargo();
	const int32 ParentCredit = GetBodyScoreCredit();
	const int32 TotalHealth = FMath::Max(1, FMath::RoundToInt(ParentHealthValue));
	const int32 HealthPerFragment = TotalHealth / FragmentCount;
	const int32 HealthRemainder = TotalHealth % FragmentCount;
	const int32 CargoPerFragment = ParentCargo / FragmentCount;
	const int32 CargoRemainder = ParentCargo % FragmentCount;
	const int32 CreditPerFragment = ParentCredit / FragmentCount;
	const int32 CreditRemainder = ParentCredit % FragmentCount;

	TArray<ASlimeEcologyEnemy*> Spawned;
	Spawned.Reserve(FragmentCount);
	bSplitting = true;
	for (int32 Index = 0; Index < FragmentCount; ++Index)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASlimeEcologyEnemy* Fragment = World->SpawnActor<ASlimeEcologyEnemy>(
			GetClass(),
			SpawnLocations[Index],
			GetActorRotation(),
			SpawnParameters);
		if (!Fragment)
		{
			for (ASlimeEcologyEnemy* Created : Spawned)
			{
				if (Created)
				{
					Created->Destroy();
				}
			}
			bSplitting = false;
			return false;
		}

		Fragment->ApplyStatRow(1);
		Fragment->InitializeEcology(
			GetFaction(),
			GetPointId(),
			GetActivityCenter(),
			GetActivityRadius());
		Fragment->GetHealthComponent()->InitializeHealth(
			FragmentRow.MaxHealth,
			(HealthPerFragment + (Index < HealthRemainder ? 1 : 0))
				/ FragmentRow.MaxHealth);

		const int32 CargoShare =
			CargoPerFragment + (Index < CargoRemainder ? 1 : 0);
		const int32 CreditShare =
			CreditPerFragment + (Index < CreditRemainder ? 1 : 0);
		Fragment->SetEcologyTotals(CargoShare, CreditShare);

		if (USlimeBodyInteractionComponent* Body = Fragment->GetBodyInteraction())
		{
			Body->ReceiveStun(Rules->SplitStunDuration);
		}
		if (USlimeEcologyFusionComponent* FragmentFusion = Fragment->GetEcologyFusionComponent())
		{
			FragmentFusion->ApplySplitCalm(Rules->FusionInterruptedBlock);
			FragmentFusion->InterruptFusion(Rules->FusionInterruptedBlock);
		}
		Spawned.Add(Fragment);
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("ImpactSplit: %s mass %d -> %d fragments; health %.1f -> %d each, ")
		TEXT("cargo %d, credit %d."),
		*GetNameSafe(this),
		FragmentCount,
		Spawned.Num(),
		ParentHealthValue,
		HealthPerFragment,
		ParentCargo,
		ParentCredit);

	bSplitting = false;
	Destroy();
	return true;
}
