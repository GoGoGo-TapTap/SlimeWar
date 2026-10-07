// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeEnemyManagerSubsystem.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Enemy/SlimeEnemyBase.h"

USlimeEnemyManagerSubsystem* USlimeEnemyManagerSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<USlimeEnemyManagerSubsystem>() : nullptr;
}

ASlimeEnemyBase* USlimeEnemyManagerSubsystem::SpawnEnemy(
	TSubclassOf<ASlimeEnemyBase> SlimeClass,
	int32 PointId,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& ActivityCenter,
	float ActivityRadius)
{
	UWorld* World = GetWorld();
	if (!World || !SlimeClass)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("USlimeEnemyManagerSubsystem::SpawnEnemy: no world or no class."));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	// The spawn point already validated the position (standable + distance to the player), so the
	// engine must not try to "fix" it behind our back.
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASlimeEnemyBase* Enemy = World->SpawnActor<ASlimeEnemyBase>(
		SlimeClass.Get(), Location, Rotation, SpawnParameters);

	if (!Enemy)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeEnemyManagerSubsystem::SpawnEnemy: spawn of %s failed at %s."),
			*GetNameSafe(SlimeClass.Get()), *Location.ToCompactString());
		return nullptr;
	}

	Enemy->InitializeFromSpawn(PointId, ActivityCenter, ActivityRadius);
	Enemy->OnEnemyDied.AddDynamic(this, &USlimeEnemyManagerSubsystem::HandleEnemyDied);

	LiveEnemies.Add(Enemy);
	PeakLiveEnemyCount = FMath::Max(PeakLiveEnemyCount, GetLiveEnemyCount());
	return Enemy;
}

int32 USlimeEnemyManagerSubsystem::DespawnAllEnemies()
{
	int32 Destroyed = 0;

	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakEnemy : LiveEnemies)
	{
		if (ASlimeEnemyBase* Enemy = WeakEnemy.Get())
		{
			Enemy->OnEnemyDied.RemoveAll(this);
			Enemy->Destroy();
			++Destroyed;
		}
	}

	LiveEnemies.Reset();
	return Destroyed;
}

int32 USlimeEnemyManagerSubsystem::GetLiveEnemyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakEnemy : LiveEnemies)
	{
		if (WeakEnemy.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

int32 USlimeEnemyManagerSubsystem::GetLiveCount(ETargetKind Kind) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakEnemy : LiveEnemies)
	{
		const ASlimeEnemyBase* Enemy = WeakEnemy.Get();
		if (Enemy && Enemy->GetTargetKind() == Kind)
		{
			++Count;
		}
	}

	return Count;
}

void USlimeEnemyManagerSubsystem::Deinitialize()
{
	LiveEnemies.Reset();
	PeakLiveEnemyCount = 0;

	Super::Deinitialize();
}

void USlimeEnemyManagerSubsystem::HandleEnemyDied(AActor* Enemy)
{
	RemoveLiveEnemy(Cast<ASlimeEnemyBase>(Enemy));
}

void USlimeEnemyManagerSubsystem::RemoveLiveEnemy(const ASlimeEnemyBase* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	LiveEnemies.RemoveAll([Enemy](const TWeakObjectPtr<ASlimeEnemyBase>& WeakEnemy)
	{
		return WeakEnemy.Get() == Enemy;
	});
}
