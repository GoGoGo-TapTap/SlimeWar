// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SpawnPoint.h"

#include "Components/CapsuleComponent.h"
#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeAggro.h"
#include "Enemy/SlimeEnemyBase.h"
#include "Enemy/SlimeNormal.h"
#include "Engine/World.h"
#include "Flow/SlimeEnemyManagerSubsystem.h"
#include "Flow/SlimeFlowMath.h"
#include "Flow/SlimeSpawnLayoutEdit.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "GameFramework/Character.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "TimerManager.h"

namespace
{
	/** Ring radius used only when the data asset has no slots at all. */
	constexpr float SlotDegradedRingRadius = 200.f;

	const TCHAR* KindName(ETargetKind Kind)
	{
		return Kind == ETargetKind::Normal ? TEXT("normal") : TEXT("aggro");
	}
}

ASpawnPoint::ASpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Pure anchor: a root, no collision, no mesh. The root must NOT be editor-only, so the visible
	// icon is a separate child component.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AnchorIcon = CreateDefaultSubobject<USlimeSpawnPointAnchor>(TEXT("AnchorIcon"));
	AnchorIcon->SetupAttachment(Root);

	NormalClass = ASlimeNormal::StaticClass();
	AggroClass = ASlimeAggro::StaticClass();
}

void ASpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRetryTimer();
	PendingSpawns.Reset();
	LiveNormals.Reset();
	LiveAggro.Reset();

	Super::EndPlay(EndPlayReason);
}

float ASpawnPoint::GetCapsuleHalfHeight(const TSubclassOf<ASlimeEnemyBase>& SlimeClass)
{
	const ASlimeEnemyBase* CDO = SlimeClass ? Cast<ASlimeEnemyBase>(SlimeClass->GetDefaultObject()) : nullptr;
	const UCapsuleComponent* Capsule = CDO ? CDO->GetCapsuleComponent() : nullptr;

	if (Capsule)
	{
		return Capsule->GetUnscaledCapsuleHalfHeight();
	}

	// Fallback: the engine's own character default, so nothing is hard coded here either.
	const ACharacter* DefaultCharacter = GetDefault<ACharacter>();
	const UCapsuleComponent* DefaultCapsule =
		DefaultCharacter ? DefaultCharacter->GetCapsuleComponent() : nullptr;

	return DefaultCapsule ? DefaultCapsule->GetUnscaledCapsuleHalfHeight() : 0.f;
}

void ASpawnPoint::InitializeRuntime(const FSlimeSpawnPointDef& InDefinition, const USlimeRunConfig& InRunConfig)
{
	Definition = InDefinition;

	BatchCount = InRunConfig.SpawnBatchCount;
	NormalPerBatch = InRunConfig.SpawnNormalPerBatch;
	AggroPerBatch = InRunConfig.SpawnAggroPerBatch;
	PlayerMinDistance = InRunConfig.SpawnPlayerMinDistance;
	RetryWindow = InRunConfig.SpawnRetryWindow;
	RetryInterval = InRunConfig.SpawnRetryInterval;
	ActivityRadius = InDefinition.ActivityRadius;
	DefaultActivityRadius = InRunConfig.AIActivityRadius;

	NormalHalfHeight = GetCapsuleHalfHeight(NormalClass);
	AggroHalfHeight = GetCapsuleHalfHeight(AggroClass);

	TotalNormalSupply = SlimeFlowMath::TotalSupply(BatchCount, NormalPerBatch);

	BatchesIssued = 0;
	bSupplyDone = false;
	SpawnedNormalCount = 0;
	SpawnedAggroCount = 0;
	LiveNormals.Reset();
	LiveAggro.Reset();
	PendingSpawns.Reset();
	StopRetryTimer();

	bRuntimeReady = BatchCount > 0 && NormalPerBatch > 0;
	State = ESpawnPointState::AwaitingDeploy;

	if (!bRuntimeReady)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("ASpawnPoint %d: SpawnBatchCount / SpawnNormalPerBatch are not filled in DA_RunConfig; ")
			TEXT("this point will never spawn."), PointId);
	}

	// A local offset of zero is a legitimate position now (it means "at the anchor"), so instead of
	// guessing which entries are "unset" we report the symptom: slots stacked on one spot.
	if (SlimeSpawnLayoutEdit::HasDuplicateXY(Definition.NormalSlots)
		|| SlimeSpawnLayoutEdit::HasDuplicateXY(Definition.AggroSlots)
		|| SlimeSpawnLayoutEdit::HasDuplicateXY(Definition.FallbackSlots))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASpawnPoint %d: two or more slots share the same XY offset in DA_SpawnLayout. ")
			TEXT("Legal, but usually a hand-typed layout - place them with the spawn point editor ")
			TEXT("in the viewport instead."), PointId);
	}

	if (RetryWindow <= 0.f || RetryInterval <= 0.f)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASpawnPoint %d: SpawnRetryWindow / SpawnRetryInterval are 0, a failed slot will be ")
			TEXT("cancelled immediately instead of retrying for 2 s."), PointId);
	}
}

float ASpawnPoint::GetEffectiveActivityRadius() const
{
	return ActivityRadius > 0.f ? ActivityRadius : DefaultActivityRadius;
}

FSlimeSpawnSlot ASpawnPoint::ResolveSlot(
	const TArray<FSlimeSpawnSlot>& Slots, int32 Index, ETargetKind Kind) const
{
	if (Slots.IsValidIndex(Index))
	{
		return Slots[Index];
	}

	// Degraded: reuse what the asset has, in order, rather than silently shrinking the batch.
	if (Slots.Num() > 0)
	{
		return Slots[Index % Slots.Num()];
	}

	// Nothing at all: spread the batch on a ring, in the anchor's local space.
	const int32 RingCount = FMath::Max(1, Kind == ETargetKind::Normal ? NormalPerBatch : AggroPerBatch);
	const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(RingCount);

	FSlimeSpawnSlot Slot;
	Slot.RelativeLocation = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * SlotDegradedRingRadius;
	return Slot;
}

void ASpawnPoint::ExecuteBatch(int32 BatchIndex)
{
	if (!bRuntimeReady)
	{
		return;
	}

	if (BatchIndex < 0 || BatchIndex >= BatchCount)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("ASpawnPoint %d: batch %d is out of range (count %d)."),
			PointId, BatchIndex, BatchCount);
		return;
	}

	if (State == ESpawnPointState::Cleared)
	{
		return;
	}

	++BatchesIssued;

	// 8 normal slots, then 2 aggro slots (design 5.2: the two kinds use different positions).
	for (int32 Index = 0; Index < NormalPerBatch; ++Index)
	{
		RequestSpawn(ETargetKind::Normal, ResolveSlot(Definition.NormalSlots, Index, ETargetKind::Normal));
	}

	for (int32 Index = 0; Index < AggroPerBatch; ++Index)
	{
		RequestSpawn(ETargetKind::Aggressive, ResolveSlot(Definition.AggroSlots, Index, ETargetKind::Aggressive));
	}

	RefreshState();
}

void ASpawnPoint::RequestSpawn(ETargetKind Kind, const FSlimeSpawnSlot& Slot)
{
	FPendingSpawn Pending;
	Pending.Kind = Kind;
	Pending.Slot = Slot;

	// The 2 s window starts when the batch is issued, not when the first retry happens.
	const UWorld* World = GetWorld();
	Pending.Deadline = (World ? World->GetTimeSeconds() : 0.f) + RetryWindow;

	ESlotRejectReason Reason = ESlotRejectReason::None;
	if (TryResolveAndSpawn(Pending, /*bLogDetail=*/true, Reason))
	{
		return;
	}

	Pending.bLoggedDetail = true;

	if (RetryWindow <= 0.f || RetryInterval <= 0.f)
	{
		// Delayed retries are disabled by the data; cancel right away (never re-issued, design 5.6).
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASpawnPoint %d: a %s spawn at local %s was dropped immediately (%s); ")
			TEXT("SpawnRetryWindow / SpawnRetryInterval are 0."),
			PointId, KindName(Kind), *Slot.RelativeLocation.ToCompactString(),
			SlimeSlotResolver::RejectReasonName(Reason));
		return;
	}

	PendingSpawns.Add(Pending);
	StartRetryTimer();
}

bool ASpawnPoint::TryResolveAndSpawn(FPendingSpawn& Pending, bool bLogDetail, ESlotRejectReason& OutReason)
{
	OutReason = ESlotRejectReason::None;

	const UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = ESlotRejectReason::NoGround;
		return false;
	}

	// Primary offset first, then the point's backup offsets in order (design 5.2).
	TArray<FSlimeSpawnSlot> Candidates;
	Candidates.Reserve(1 + Definition.FallbackSlots.Num());
	Candidates.Add(Pending.Slot);
	Candidates.Append(Definition.FallbackSlots);

	const FTransform AnchorTransform = GetActorTransform();
	const float HalfHeight = (Pending.Kind == ETargetKind::Normal) ? NormalHalfHeight : AggroHalfHeight;

	FString Detail;
	if (bLogDetail)
	{
		Detail.Reserve(256);
	}

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FSlimeSpawnSlot& Candidate = Candidates[Index];

		// The offset is anchor-local, so moving or rotating the anchor moves the whole layout.
		const FVector DesiredWorld = AnchorTransform.TransformPosition(Candidate.RelativeLocation);
		const SlimeSlotResolver::FResolution Resolution =
			SlimeSlotResolver::Resolve(*World, this, DesiredWorld, PlayerMinDistance);

		if (Resolution.bValid)
		{
			// The desired Z was only a hint: the slime stands on the ground found under its XY.
			const FVector SpawnLocation(
				Resolution.Location.X,
				Resolution.Location.Y,
				SlimeSlotResolver::GetSpawnZ(Resolution.GroundZ, HalfHeight));

			const FRotator SpawnRotation =
				(AnchorTransform.GetRotation() * Candidate.RelativeRotation.Quaternion()).Rotator();

			if (SpawnOne(Pending.Kind, SpawnLocation, SpawnRotation))
			{
				return true;
			}

			OutReason = ESlotRejectReason::Blocked;
		}
		else
		{
			OutReason = Resolution.Reason;
		}

		if (bLogDetail)
		{
			const FString CandidateLabel = (Index == 0)
				? FString(TEXT("primary"))
				: FString::Printf(TEXT("fallback %d"), Index - 1);

			Detail += FString::Printf(
				TEXT("\n  %s at %s -> %s (ground Z %.0f, player %.0f cm)"),
				*CandidateLabel,
				*DesiredWorld.ToCompactString(),
				SlimeSlotResolver::RejectReasonName(OutReason),
				Resolution.GroundZ,
				Resolution.PlayerDistance);
		}
	}

	if (bLogDetail)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASpawnPoint %d: a %s spawn could not be placed (local %s):%s"),
			PointId, KindName(Pending.Kind),
			*Pending.Slot.RelativeLocation.ToCompactString(),
			*Detail);
	}

	return false;
}

ASlimeEnemyBase* ASpawnPoint::SpawnOne(
	ETargetKind Kind, const FVector& Location, const FRotator& Rotation)
{
	USlimeEnemyManagerSubsystem* Manager = USlimeEnemyManagerSubsystem::Get(this);
	if (!Manager)
	{
		return nullptr;
	}

	const TSubclassOf<ASlimeEnemyBase> Class =
		(Kind == ETargetKind::Normal) ? NormalClass : AggroClass;

	// The activity centre is the point, never the slot: normal slimes must roam around the point
	// and only fuse with slimes of the same point (design 5.4).
	// The radius is this point's effective value, so design 5.1 ("normal targets only act inside
	// their own point") is enforced by the AI, not just drawn in the editor.
	ASlimeEnemyBase* Enemy = Manager->SpawnEnemy(
		Class, PointId, Location, Rotation, GetActorLocation(), GetEffectiveActivityRadius());
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->OnEnemyDied.AddDynamic(this, &ASpawnPoint::HandleEnemyDied);

	if (Kind == ETargetKind::Normal)
	{
		++SpawnedNormalCount;
		LiveNormals.Add(Enemy);
	}
	else
	{
		++SpawnedAggroCount;
		LiveAggro.Add(Enemy);
	}

	return Enemy;
}

void ASpawnPoint::StartRetryTimer()
{
	if (RetryTimerHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RetryTimerHandle, this, &ASpawnPoint::TickRetryQueue, RetryInterval, /*bLoop=*/true);
}

void ASpawnPoint::StopRetryTimer()
{
	if (!RetryTimerHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}

	RetryTimerHandle.Invalidate();
}

void ASpawnPoint::StopSpawning()
{
	StopRetryTimer();
	PendingSpawns.Reset();
	RefreshState();
}

void ASpawnPoint::TickRetryQueue()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const FTransform AnchorTransform = GetActorTransform();

	for (int32 Index = PendingSpawns.Num() - 1; Index >= 0; --Index)
	{
		FPendingSpawn& Pending = PendingSpawns[Index];
		ESlotRejectReason Reason = ESlotRejectReason::None;

		if (TryResolveAndSpawn(Pending, /*bLogDetail=*/!Pending.bLoggedDetail, Reason))
		{
			PendingSpawns.RemoveAt(Index);
			continue;
		}

		Pending.bLoggedDetail = true;

		if (Now >= Pending.Deadline)
		{
			// Design 5.6: a cancelled spawn is never re-issued later, and it does not shrink the
			// supply either - the point can still be cleared.
			UE_LOG(LogSlimeWar, Warning,
				TEXT("ASpawnPoint %d: a %s spawn (local %s -> world %s) was cancelled after %.1f s - %s; ")
				TEXT("no fallback worked."),
				PointId,
				KindName(Pending.Kind),
				*Pending.Slot.RelativeLocation.ToCompactString(),
				*AnchorTransform.TransformPosition(Pending.Slot.RelativeLocation).ToCompactString(),
				RetryWindow,
				SlimeSlotResolver::RejectReasonName(Reason));

			PendingSpawns.RemoveAt(Index);
		}
	}

	if (PendingSpawns.Num() == 0)
	{
		StopRetryTimer();
	}

	RefreshState();
}

void ASpawnPoint::HandleEnemyDied(AActor* Enemy)
{
	const ASlimeEnemyBase* Slime = Cast<ASlimeEnemyBase>(Enemy);
	if (!Slime)
	{
		return;
	}

	const auto IsSame = [Slime](const TWeakObjectPtr<ASlimeEnemyBase>& WeakSlime)
	{
		return WeakSlime.Get() == Slime;
	};

	LiveNormals.RemoveAll(IsSame);
	LiveAggro.RemoveAll(IsSame);

	RefreshState();
}

int32 ASpawnPoint::GetLiveNormalCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakSlime : LiveNormals)
	{
		if (WeakSlime.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

int32 ASpawnPoint::GetLiveAggroCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASlimeEnemyBase>& WeakSlime : LiveAggro)
	{
		if (WeakSlime.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

void ASpawnPoint::RefreshState()
{
	// Supply is done once every batch has been issued and nothing is still waiting out its window.
	bSupplyDone = (BatchesIssued >= BatchCount) && PendingSpawns.Num() == 0;

	SetState(SlimeFlowMath::EvaluatePointState(bSupplyDone, BatchesIssued > 0, GetLiveNormalCount()));
}

void ASpawnPoint::SetState(ESpawnPointState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;

	// The director is the only cross-module channel (plan rule 6 / red line C18).
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnPointStateChanged(PointId, NewState);
	}
	else
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASpawnPoint %d: no battle director, the state change was not reported."), PointId);
	}
}
