// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologySubsystem.h"

#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Engine/GameInstance.h"
#include "GameplayFramework/StatTableProvider.h"

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

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator < RefreshInterval)
	{
		return;
	}

	RefreshAccumulator = FMath::Fmod(RefreshAccumulator, RefreshInterval);
	RebuildBuckets();
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
