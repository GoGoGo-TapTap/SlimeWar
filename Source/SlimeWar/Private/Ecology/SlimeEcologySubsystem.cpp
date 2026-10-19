// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologySubsystem.h"

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeCargoPickup.h"
#include "Ecology/SlimeCoreComponent.h"
#include "Ecology/SlimeEcologyBrainComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeFactionSite.h"
#include "Ecology/SlimeMineralPatch.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

namespace
{
	const USlimeEcologyRules* GetRules(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UStatTableProvider* Provider =
			GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
		return Provider && Provider->GetEcologyRules()
			? Provider->GetEcologyRules()
			: GetDefault<USlimeEcologyRules>();
	}
}

void USlimeEcologySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const USlimeEcologyRules* Rules = GetRules(GetWorld()))
	{
		RefreshInterval = FMath::Max(0.05f, Rules->PerceptionInterval);
		CellSize = FMath::Max(100.f, Rules->SpatialCellSize);
	}

	RegisteredAgents.Reset();
	Buckets.Reset();
	bBucketsDirty = true;
}

void USlimeEcologySubsystem::Deinitialize()
{
	RegisteredAgents.Reset();
	Buckets.Reset();
	Super::Deinitialize();
}

TStatId USlimeEcologySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USlimeEcologySubsystem, STATGROUP_Tickables);
}

void USlimeEcologySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRunActive)
	{
		ProcessSpawnQueue();
		EvaluateExtinction(DeltaTime);

		LedgerLogAccumulator += DeltaTime;
		if (LedgerLogAccumulator >= 10.f)
		{
			LedgerLogAccumulator = 0.f;
			CheckLedger(/*bLogResult=*/true, TEXT("periodic"));
		}

		FallProbeAccumulator += DeltaTime;
		if (FallProbeAccumulator >= 1.f)
		{
			FallProbeAccumulator = 0.f;
			for (const TWeakObjectPtr<ASlimeEcologyEnemy>& Entry : RegisteredAgents)
			{
				const ASlimeEcologyEnemy* Agent = Entry.Get();
				const USlimeHealthComponent* Health = Agent ? Agent->GetHealthComponent() : nullptr;
				if (Agent && (!Health || !Health->IsDead()) && Agent->GetActorLocation().Z < -200.f)
				{
					UE_LOG(LogSlimeWar, Warning,
						TEXT("EcologyFallProbe: %s faction=%d site=%s loc=%s vel=%s behaviour=%s goal=%s."),
						*GetNameSafe(Agent),
						static_cast<int32>(Agent->GetFaction()),
						Agent->GetAssignedSite() ? *Agent->GetAssignedSite()->SiteId.ToString() : TEXT("None"),
						*Agent->GetActorLocation().ToCompactString(),
						*Agent->GetVelocity().ToCompactString(),
						Agent->GetEcologyBrain() ? Agent->GetEcologyBrain()->GetBehaviorName() : TEXT("None"),
						Agent->GetEcologyBrain()
							? *Agent->GetEcologyBrain()->GetDesiredGoal().ToCompactString()
							: TEXT("None"));
				}
			}
		}
	}

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator < RefreshInterval)
	{
		return;
	}

	RefreshAccumulator = FMath::Fmod(RefreshAccumulator, RefreshInterval);
	RebuildBuckets();
}

void USlimeEcologySubsystem::RegisterSite(ASlimeFactionSite* Site)
{
	if (Site)
	{
		Sites.AddUnique(Site);
	}
}

void USlimeEcologySubsystem::UnregisterSite(ASlimeFactionSite* Site)
{
	Sites.RemoveAll([Site](const TObjectPtr<ASlimeFactionSite>& Entry)
	{
		return !Entry || Entry == Site;
	});
	PendingSpawns.RemoveAll([Site](const FPendingSpawn& Request)
	{
		return !Request.Site.IsValid() || Request.Site.Get() == Site;
	});
}

void USlimeEcologySubsystem::RegisterMineralPatch(ASlimeMineralPatch* Patch)
{
	if (Patch)
	{
		MineralPatches.AddUnique(Patch);
	}
}

void USlimeEcologySubsystem::UnregisterMineralPatch(ASlimeMineralPatch* Patch)
{
	MineralPatches.RemoveAll([Patch](const TObjectPtr<ASlimeMineralPatch>& Entry)
	{
		return !Entry || Entry == Patch;
	});
}

void USlimeEcologySubsystem::RegisterCargoPickup(ASlimeCargoPickup* Pickup)
{
	if (Pickup)
	{
		CargoPickups.AddUnique(Pickup);
	}
}

void USlimeEcologySubsystem::UnregisterCargoPickup(ASlimeCargoPickup* Pickup)
{
	CargoPickups.RemoveAll([Pickup](const TObjectPtr<ASlimeCargoPickup>& Entry)
	{
		return !Entry || Entry == Pickup;
	});
}

void USlimeEcologySubsystem::BeginRun()
{
	if (bRunActive)
	{
		return;
	}

	bRunActive = true;
	bExtinctionReported = false;
	ExtinctionCheckAccumulator = 0.f;
	LedgerLogAccumulator = 0.f;
	DeliveredCargo = 0;
	RemovedMass = 0;
	InitialMass = 0;
	PendingSpawns.Reset();

	for (ASlimeFactionSite* Site : Sites)
	{
		if (!Site)
		{
			continue;
		}

		InitialMass += FMath::Max(0, Site->InitialPopulation);
		for (int32 Index = 0; Index < Site->InitialPopulation; ++Index)
		{
			PendingSpawns.Add({Site});
		}
	}

	// Process the whole initial population before the first ledger check. The double-point level
	// has 20 bodies and the three-point level has 30; a fixed three-pass loop only drains 24 and
	// makes the begin-run mass ledger look unbalanced until the next tick.
	int32 SpawnPasses = 0;
	const int32 MaxSpawnPasses = InitialMass + 16;
	while (PendingSpawns.Num() > 0 && SpawnPasses++ < MaxSpawnPasses)
	{
		ProcessSpawnQueue();
	}

	if (PendingSpawns.Num() > 0)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("EcologySubsystem: %d initial spawn request(s) are still pending after %d passes."),
			PendingSpawns.Num(),
			SpawnPasses);
	}

	CheckLedger(/*bLogResult=*/true, TEXT("begin run"));
}

void USlimeEcologySubsystem::EndRun()
{
	bRunActive = false;
	PendingSpawns.Reset();
	CheckLedger(/*bLogResult=*/true, TEXT("end run"));
}

bool USlimeEcologySubsystem::HasEcologySites() const
{
	for (const ASlimeFactionSite* Site : Sites)
	{
		if (Site)
		{
			return true;
		}
	}
	return false;
}

bool USlimeEcologySubsystem::TryGather(
	ASlimeEcologyEnemy* Enemy,
	ASlimeMineralPatch* Patch)
{
	const USlimeEcologyRules* Rules = GetRules(GetWorld());
	if (!bRunActive || !Enemy || !Patch || !Rules || Enemy->GetCargo() > 0 || Patch->Stock <= 0)
	{
		return false;
	}
	if (FVector::Dist2D(Enemy->GetActorLocation(), Patch->GetActorLocation())
		> Rules->GatherRange + 10.f)
	{
		return false;
	}

	if (Patch->ConsumeOne() != 1)
	{
		return false;
	}

	Enemy->AddCargo(1);
	return true;
}

bool USlimeEcologySubsystem::TryDeliver(ASlimeEcologyEnemy* Enemy)
{
	const USlimeEcologyRules* Rules = GetRules(GetWorld());
	ASlimeFactionSite* Site = Enemy ? Enemy->GetAssignedSite() : nullptr;
	if (!bRunActive || !Enemy || !Site || !Rules || Enemy->GetCargo() <= 0
		|| Site->Faction != Enemy->GetFaction()
		|| FVector::Dist2D(Enemy->GetActorLocation(), Site->GetDeliveryLocation())
			> FMath::Max(Rules->DeliveryRange, Site->DeliveryRadius))
	{
		return false;
	}

	if (!Enemy->ConsumeCargo(1))
	{
		return false;
	}

	++DeliveredCargo;
	Site->NotifyDelivered();
	PendingSpawns.Add({Site});
	return true;
}

bool USlimeEcologySubsystem::TryPickupCargo(
	ASlimeEcologyEnemy* Enemy,
	ASlimeCargoPickup* Pickup)
{
	const USlimeEcologyRules* Rules = GetRules(GetWorld());
	if (!bRunActive || !Enemy || !Pickup || !Rules || Enemy->GetCargo() > 0
		|| Pickup->GetAmount() <= 0
		|| FVector::Dist2D(Enemy->GetActorLocation(), Pickup->GetActorLocation())
			> Rules->PickupRange + 10.f)
	{
		return false;
	}

	if (Pickup->TakeOne() != 1)
	{
		return false;
	}

	Enemy->AddCargo(1);
	return true;
}

bool USlimeEcologySubsystem::DebugGrantCargoFromStock(ASlimeEcologyEnemy* Enemy)
{
	if (!bRunActive || !Enemy)
	{
		return false;
	}

	ASlimeMineralPatch* Patch = FindBestMineralPatch(Enemy, 100000.f);
	if (!Patch || Patch->ConsumeOne() != 1)
	{
		return false;
	}

	Enemy->AddCargo(1);
	return true;
}

void USlimeEcologySubsystem::DropCargo(const FVector& Location, int32 Amount)
{
	UWorld* World = GetWorld();
	if (!bRunActive || !World || Amount <= 0)
	{
		return;
	}

	FVector SpawnLocation = Location;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeCargoDrop), false);
	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
		GroundHit,
		SpawnLocation + FVector(0.f, 0.f, 80.f),
		SpawnLocation - FVector(0.f, 0.f, 1000.f),
		ECC_WorldStatic,
		QueryParams))
	{
		SpawnLocation.Z = GroundHit.ImpactPoint.Z + 18.f;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ASlimeCargoPickup* Pickup = World->SpawnActor<ASlimeCargoPickup>(
		ASlimeCargoPickup::StaticClass(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters))
	{
		Pickup->SetAmount(Amount);
		UE_LOG(LogSlimeWar, Log,
			TEXT("EcologyCargo: dropped %d at %s."),
			Amount,
			*SpawnLocation.ToCompactString());
	}
}

void USlimeEcologySubsystem::NotifyAgentRemoved(
	const ASlimeEcologyEnemy* Enemy,
	int32 InRemovedMass)
{
	if (!Enemy || InRemovedMass <= 0)
	{
		return;
	}

	RemovedMass += InRemovedMass;
}

ASlimeMineralPatch* USlimeEcologySubsystem::FindBestMineralPatch(
	const ASlimeEcologyEnemy* Enemy,
	float MaxRange) const
{
	if (!Enemy)
	{
		return nullptr;
	}

        ASlimeMineralPatch* Best = nullptr;
        float BestScore = MaxRange;
        const USlimeEcologyRules* Rules = GetRules(GetWorld());
        const UWorld* World = GetWorld();
	for (ASlimeMineralPatch* Patch : MineralPatches)
	{
		if (!Patch || Patch->Stock <= 0)
		{
			continue;
		}

                const float Distance =
                        FVector::Dist2D(Enemy->GetActorLocation(), Patch->GetActorLocation());
                if (Distance > MaxRange)
		{
                        continue;
                }

                float Crowding = 0.f;
                if (Rules && World)
                {
                        TArray<ASlimeEcologyEnemy*> Neighbours;
                        QueryAgentsAroundLocation(
                                Patch->GetActorLocation(),
                                Rules->MineralCrowdingRadius,
                                Neighbours);
                        for (const ASlimeEcologyEnemy* Neighbour : Neighbours)
                        {
                                if (!Neighbour || Neighbour == Enemy
                                        || Neighbour->GetFaction() != Enemy->GetFaction())
                                {
                                        continue;
                                }

                                FCollisionQueryParams Params(
                                        SCENE_QUERY_STAT(SlimeMineralCrowdingSight), false, Enemy);
                                Params.AddIgnoredActor(Neighbour);
                                FHitResult Hit;
                                if (!World->LineTraceSingleByChannel(
                                        Hit,
                                        Enemy->GetActorLocation(),
                                        Neighbour->GetActorLocation(),
                                        ECC_WorldStatic,
                                        Params))
                                {
                                        Crowding += 1.f;
                                }
                        }
                }

                const float Score =
                        Distance + Crowding * (Rules ? Rules->MineralCrowdingPenalty * 100.f : 0.f);
                if (Score < BestScore)
                {
                        BestScore = Score;
			Best = Patch;
		}
	}
	return Best;
}

ASlimeCargoPickup* USlimeEcologySubsystem::FindNearestCargoPickup(
	const ASlimeEcologyEnemy* Enemy,
	float MaxRange) const
{
	if (!Enemy)
	{
		return nullptr;
	}

	ASlimeCargoPickup* Best = nullptr;
	float BestDistanceSq = FMath::Square(MaxRange);
	for (ASlimeCargoPickup* Pickup : CargoPickups)
	{
		if (!Pickup || Pickup->GetAmount() <= 0)
		{
			continue;
		}

		const float DistanceSq =
			FVector::DistSquared2D(Enemy->GetActorLocation(), Pickup->GetActorLocation());
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Pickup;
		}
	}
	return Best;
}

int32 USlimeEcologySubsystem::GetLiveMass() const
{
	int32 Mass = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : RegisteredAgents)
	{
		if (const ASlimeEcologyEnemy* Agent = WeakAgent.Get())
		{
			Mass += Agent->GetMass();
		}
	}
	return Mass;
}

int32 USlimeEcologySubsystem::GetCarriedCargo() const
{
	int32 Cargo = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : RegisteredAgents)
	{
		if (const ASlimeEcologyEnemy* Agent = WeakAgent.Get())
		{
			Cargo += Agent->GetCargo();
		}
	}
	return Cargo;
}

int32 USlimeEcologySubsystem::GetGroundCargo() const
{
	int32 Cargo = 0;
	for (const ASlimeCargoPickup* Pickup : CargoPickups)
	{
		if (Pickup)
		{
			Cargo += Pickup->GetAmount();
		}
	}
	return Cargo;
}

int32 USlimeEcologySubsystem::GetPatchStock() const
{
	int32 Stock = 0;
	for (const ASlimeMineralPatch* Patch : MineralPatches)
	{
		if (Patch)
		{
			Stock += Patch->Stock;
		}
	}
	return Stock;
}

int32 USlimeEcologySubsystem::GetRegeneratedStock() const
{
	int32 Regenerated = 0;
	for (const ASlimeMineralPatch* Patch : MineralPatches)
	{
		if (Patch)
		{
			Regenerated += Patch->GetRegeneratedCount();
		}
	}
	return Regenerated;
}

TArray<FSlimeSiteStatus> USlimeEcologySubsystem::GetSiteStatuses() const
{
	TArray<FSlimeSiteStatus> Result;
	for (const ASlimeFactionSite* Site : Sites)
	{
		if (!Site)
		{
			continue;
		}

		FSlimeSiteStatus Status;
		Status.SiteId = Site->SiteId;
		Status.Faction = Site->Faction;
		Status.Location = Site->GetActorLocation();
		Status.DeliveredCount = Site->GetDeliveredCount();
		for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : RegisteredAgents)
		{
			const ASlimeEcologyEnemy* Agent = WeakAgent.Get();
                        if (!Agent || Agent->GetFaction() != Site->Faction)
			{
				continue;
			}

			++Status.LiveCount;
			Status.LiveMass += Agent->GetMass();
			if (Agent->GetCoreComponent() && Agent->GetCoreComponent()->IsCore())
			{
				++Status.CoreCount;
			}
		}
		for (const FPendingSpawn& Request : PendingSpawns)
		{
			if (Request.Site.Get() == Site)
			{
				++Status.PendingSpawns;
			}
		}
		Result.Add(Status);
	}

	Result.Sort([](const FSlimeSiteStatus& A, const FSlimeSiteStatus& B)
	{
		return static_cast<uint8>(A.Faction) < static_cast<uint8>(B.Faction);
	});
	return Result;
}

TArray<FSlimeMineralStatus> USlimeEcologySubsystem::GetMineralStatuses() const
{
	TArray<FSlimeMineralStatus> Result;
	for (const ASlimeMineralPatch* Patch : MineralPatches)
	{
		if (!Patch)
		{
			continue;
		}

		FSlimeMineralStatus Status;
		Status.PatchId = Patch->PatchId;
		Status.Location = Patch->GetActorLocation();
		Status.Stock = Patch->Stock;
		Status.InitialStock = Patch->InitialStock;
		Status.MaxStock = Patch->MaxStock;
		Status.bRegenerates = Patch->bRegenerates;
		Status.RegrowRemaining = Patch->GetRegrowRemaining();
		Result.Add(Status);
	}

	Result.Sort([](const FSlimeMineralStatus& A, const FSlimeMineralStatus& B)
	{
		return A.PatchId.ToString() < B.PatchId.ToString();
	});
	return Result;
}

FSlimeEcologyLedger USlimeEcologySubsystem::BuildLedger() const
{
	FSlimeEcologyLedger Ledger;
	Ledger.InitialStock = 0;
	for (const ASlimeMineralPatch* Patch : MineralPatches)
	{
		if (Patch)
		{
			Ledger.InitialStock += Patch->InitialStock;
		}
	}
	Ledger.RegeneratedStock = GetRegeneratedStock();
	Ledger.StockInPatches = GetPatchStock();
	Ledger.CargoCarried = GetCarriedCargo();
	Ledger.CargoOnGround = GetGroundCargo();
	Ledger.DeliveredCargo = DeliveredCargo;
	Ledger.InitialMass = InitialMass;
	Ledger.DeliveredMass = DeliveredCargo;
	Ledger.LiveMass = GetLiveMass();
	Ledger.RemovedMass = RemovedMass;
	return Ledger;
}

bool USlimeEcologySubsystem::CheckLedger(
	bool bLogResult,
	const TCHAR* Context) const
{
	const FSlimeEcologyLedger Ledger = BuildLedger();
	const bool bResourceOk = Ledger.IsResourceBalanced();
	const bool bMassOk = Ledger.IsMassBalanced();
	if (bLogResult)
	{
		const FString Message = FString::Printf(
			TEXT("EcologyLedger[%s]: resource %s (%d stock + %d carried + %d ground + %d delivered ")
			TEXT("= %d initial + %d regen); mass %s (%d initial + %d delivered = %d live + %d removed); ")
			TEXT("pending=%d"),
			Context ? Context : TEXT("check"),
			bResourceOk ? TEXT("OK") : TEXT("BROKEN"),
			Ledger.StockInPatches,
			Ledger.CargoCarried,
			Ledger.CargoOnGround,
			Ledger.DeliveredCargo,
			Ledger.InitialStock,
			Ledger.RegeneratedStock,
			bMassOk ? TEXT("OK") : TEXT("BROKEN"),
			Ledger.InitialMass,
			Ledger.DeliveredMass,
			Ledger.LiveMass,
			Ledger.RemovedMass,
			PendingSpawns.Num());
		if (bResourceOk && bMassOk)
		{
			UE_LOG(LogSlimeWar, Log, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogSlimeWar, Warning, TEXT("%s"), *Message);
		}
	}
	return bResourceOk && bMassOk;
}

void USlimeEcologySubsystem::RegisterAgent(ASlimeEcologyEnemy* Agent)
{
	if (!Agent)
	{
		return;
	}

	RegisteredAgents.AddUnique(Agent);
	bBucketsDirty = true;
}

void USlimeEcologySubsystem::UnregisterAgent(ASlimeEcologyEnemy* Agent)
{
	RegisteredAgents.RemoveAll([Agent](const TWeakObjectPtr<ASlimeEcologyEnemy>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Agent;
	});
	bBucketsDirty = true;
}

void USlimeEcologySubsystem::QueryNeighbors(
	const ASlimeEcologyEnemy* Source,
	float Radius,
	TArray<ASlimeEcologyEnemy*>& OutNeighbors) const
{
	OutNeighbors.Reset();
	if (!Source)
	{
		return;
	}

	if (bBucketsDirty)
	{
		RebuildBuckets();
	}

	const FIntPoint CentreCell = ToCell(Source->GetActorLocation());
	const int32 CellRadius = FMath::CeilToInt(FMath::Max(0.f, Radius) / CellSize);
	const float RadiusSq = FMath::Square(FMath::Max(0.f, Radius));
	int32 VisitedCells = 0;

	for (int32 X = CentreCell.X - CellRadius; X <= CentreCell.X + CellRadius; ++X)
	{
		for (int32 Y = CentreCell.Y - CellRadius; Y <= CentreCell.Y + CellRadius; ++Y)
		{
			++VisitedCells;
			const TArray<TWeakObjectPtr<ASlimeEcologyEnemy>>* Cell =
				Buckets.Find(FIntPoint(X, Y));
			if (!Cell)
			{
				continue;
			}

			for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : *Cell)
			{
				ASlimeEcologyEnemy* Agent = WeakAgent.Get();
				if (!Agent || Agent == Source)
				{
					continue;
				}

				if (FVector::DistSquared2D(Source->GetActorLocation(), Agent->GetActorLocation()) <= RadiusSq)
				{
					OutNeighbors.AddUnique(Agent);
				}
			}
		}
	}

	LastQueryVisitedCells = VisitedCells;
}

void USlimeEcologySubsystem::QueryAgentsAroundLocation(
	const FVector& Location,
	float Radius,
	TArray<ASlimeEcologyEnemy*>& OutNeighbors) const
{
	OutNeighbors.Reset();
	if (bBucketsDirty)
	{
		RebuildBuckets();
	}

	const FIntPoint CentreCell = ToCell(Location);
	const int32 CellRadius = FMath::CeilToInt(FMath::Max(0.f, Radius) / CellSize);
	const float RadiusSq = FMath::Square(FMath::Max(0.f, Radius));
	int32 VisitedCells = 0;

	for (int32 X = CentreCell.X - CellRadius; X <= CentreCell.X + CellRadius; ++X)
	{
		for (int32 Y = CentreCell.Y - CellRadius; Y <= CentreCell.Y + CellRadius; ++Y)
		{
			++VisitedCells;
			const TArray<TWeakObjectPtr<ASlimeEcologyEnemy>>* Cell =
				Buckets.Find(FIntPoint(X, Y));
			if (!Cell)
			{
				continue;
			}

			for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : *Cell)
			{
				ASlimeEcologyEnemy* Agent = WeakAgent.Get();
				if (Agent && FVector::DistSquared2D(Location, Agent->GetActorLocation()) <= RadiusSq)
				{
					OutNeighbors.AddUnique(Agent);
				}
			}
		}
	}

	LastQueryVisitedCells = VisitedCells;
}

int32 USlimeEcologySubsystem::GetRegisteredCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& Entry : RegisteredAgents)
	{
		Count += Entry.IsValid() ? 1 : 0;
	}
	return Count;
}

ASlimeFactionSite* USlimeEcologySubsystem::FindSiteForFaction(ESlimeFaction Faction) const
{
	for (ASlimeFactionSite* Site : Sites)
	{
		if (Site && Site->Faction == Faction)
		{
			return Site;
		}
	}
	return nullptr;
}

ASlimeFactionSite* USlimeEcologySubsystem::GetSiteForFaction(ESlimeFaction Faction) const
{
        return FindSiteForFaction(Faction);
}

bool USlimeEcologySubsystem::HasFactionSite(ESlimeFaction Faction) const
{
	return FindSiteForFaction(Faction) != nullptr;
}

FVector USlimeEcologySubsystem::ResolveSiteSpawnLocation(ASlimeFactionSite* Site) const
{
	UWorld* World = GetWorld();
	const USlimeEcologyRules* Rules = GetRules(World);
	if (!World || !Site || !Rules)
	{
		return FVector::ZeroVector;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Centre = Site->GetActorLocation();
	const float RingMin = FMath::Max(0.f, Site->SpawnRingMin > 0.f
		? Site->SpawnRingMin
		: Rules->SiteSpawnRingMin);
	const float RingMax = FMath::Max(RingMin, Site->SpawnRingMax > 0.f
		? Site->SpawnRingMax
		: Rules->SiteSpawnRingMax);
	int32 NavFailures = 0;
	int32 PlayerFailures = 0;
	int32 StaticFailures = 0;
	int32 AgentFailures = 0;

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Ring = FMath::FRandRange(RingMin, RingMax);
		FVector Candidate = Centre + FVector(
			FMath::Cos(Angle) * Ring,
			FMath::Sin(Angle) * Ring,
			0.f);

		FNavLocation Projected;
		const bool bProjected = NavSystem && NavSystem->ProjectPointToNavigation(
			Candidate,
			Projected,
			FVector(250.f, 250.f, 500.f));
		if (!bProjected)
		{
			++NavFailures;
		}
		else
		{
			Candidate = Projected.Location;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SlimeSiteSpawn), false);
		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(
			GroundHit,
			Candidate + FVector(0.f, 0.f, 100.f),
			Candidate - FVector(0.f, 0.f, 1000.f),
			ECC_WorldStatic,
			QueryParams))
		{
			Candidate.Z = GroundHit.ImpactPoint.Z + 22.f;
		}

		if (Player && FVector::Dist2D(Candidate, Player->GetActorLocation())
			< Rules->SiteSpawnPlayerMinDistance)
		{
			++PlayerFailures;
			continue;
		}

		if (World->OverlapBlockingTestByChannel(
			Candidate,
			FQuat::Identity,
			ECC_WorldStatic,
			FCollisionShape::MakeSphere(18.f),
			QueryParams))
		{
			++StaticFailures;
			continue;
		}

		TArray<ASlimeEcologyEnemy*> Neighbours;
		QueryAgentsAroundLocation(Candidate, 80.f, Neighbours);
		if (Neighbours.Num() > 0)
		{
			++AgentFailures;
			continue;
		}

		return Candidate;
	}

	UE_LOG(LogSlimeWar, Warning,
		TEXT("EcologySpawn: site %s found no location (nav=%d player=%d static=%d agents=%d)."),
		*Site->SiteId.ToString(),
		NavFailures,
		PlayerFailures,
		StaticFailures,
		AgentFailures);
	return FVector::ZeroVector;
}

bool USlimeEcologySubsystem::SpawnOneAtSite(ASlimeFactionSite* Site)
{
	UWorld* World = GetWorld();
	if (!World || !Site)
	{
		return false;
	}

	const FVector Location = ResolveSiteSpawnLocation(Site);
	if (Location.IsNearlyZero())
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASlimeEcologyEnemy* Enemy = World->SpawnActor<ASlimeEcologyEnemy>(
		ASlimeEcologyEnemy::StaticClass(),
		Location,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!Enemy)
	{
		return false;
	}

	Enemy->ApplyStatRow(1);
	Enemy->InitializeEcology(
		Site->Faction,
		static_cast<int32>(GetTypeHash(Site->SiteId) & 0x7fffffff),
		Site->GetActorLocation(),
		Site->ActivityRadius);
	Enemy->SetAssignedSite(Site);
	return true;
}

void USlimeEcologySubsystem::ProcessSpawnQueue()
{
	if (!bRunActive || PendingSpawns.Num() == 0)
	{
		return;
	}

	const USlimeEcologyRules* Rules = GetRules(GetWorld());
	const int32 MaxPerFrame = Rules ? FMath::Max(1, Rules->SpawnRequestsPerFrame) : 8;
	const int32 Attempts = FMath::Min(MaxPerFrame, PendingSpawns.Num());

	for (int32 Attempt = 0; Attempt < Attempts && PendingSpawns.Num() > 0; ++Attempt)
	{
		const int32 Index = PendingSpawns.Num() - 1;
		ASlimeFactionSite* Site = PendingSpawns[Index].Site.Get();
		if (!Site || SpawnOneAtSite(Site))
		{
			PendingSpawns.RemoveAt(Index);
		}
	}
}

void USlimeEcologySubsystem::EvaluateExtinction(float DeltaTime)
{
	if (!bRunActive || bExtinctionReported)
	{
		return;
	}

	ExtinctionCheckAccumulator += DeltaTime;
	if (ExtinctionCheckAccumulator < 0.5f)
	{
		return;
	}
	ExtinctionCheckAccumulator = 0.f;

	if (GetRegisteredCount() > 0 || PendingSpawns.Num() > 0 || GetGroundCargo() > 0)
	{
		return;
	}

	bExtinctionReported = true;
	OnExtinction.Broadcast();
	UE_LOG(LogSlimeWar, Log, TEXT("EcologySubsystem: extinction detected."));
}

FIntPoint USlimeEcologySubsystem::ToCell(const FVector& Location) const
{
	return FIntPoint(
		FMath::FloorToInt(Location.X / CellSize),
		FMath::FloorToInt(Location.Y / CellSize));
}

void USlimeEcologySubsystem::RebuildBuckets() const
{
	Buckets.Reset();

	for (const TWeakObjectPtr<ASlimeEcologyEnemy>& WeakAgent : RegisteredAgents)
	{
		ASlimeEcologyEnemy* Agent = WeakAgent.Get();
		if (!Agent || Agent->IsActorBeingDestroyed())
		{
			continue;
		}

		Buckets.FindOrAdd(ToCell(Agent->GetActorLocation())).Add(Agent);
	}

	bBucketsDirty = false;
	++RebuildCount;
}
