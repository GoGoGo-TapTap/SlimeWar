// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSpawnEditorUtils.h"

#include "Core/SlimeWarLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/SlimeSlotResolver.h"
#include "Flow/SlimeSpawnLayoutEdit.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "Flow/SpawnPoint.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeRunConfig.h"

namespace
{
	/**
	 * Ring radii of the freshly created default layout, cm.
	 * Tool defaults for a blank point (not gameplay values): they only seed the handles, which the
	 * designer then drags. Every radius stays inside the design's 6 m activity radius.
	 */
	constexpr float DefaultNormalRingRadius = 450.f;
	constexpr float DefaultAggroRingRadius = 650.f;
	constexpr float DefaultFallbackRingRadius = 850.f;
	constexpr int32 DefaultFallbackCount = 3;

	/** Positions closer than this count as "the same slot" when comparing level vs asset. */
	constexpr float SlotMatchTolerance = 0.1f;

	bool SlotsMatch(const TArray<FSlimeSpawnSlot>& A, const TArray<FSlimeSpawnSlot>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].RelativeLocation.Equals(B[Index].RelativeLocation, SlotMatchTolerance))
			{
				return false;
			}
		}

		return true;
	}

	int32 NextFreeIndex(const ASpawnPoint& Point, ESlimeSlotRole Role)
	{
		TArray<USlimeSpawnSlotMarker*> Markers;
		SlimeSpawnEditor::CollectMarkers(Point, Markers);

		int32 Highest = -1;
		for (const USlimeSpawnSlotMarker* Marker : Markers)
		{
			if (Marker && Marker->Role == Role)
			{
				Highest = FMath::Max(Highest, Marker->SlotIndex);
			}
		}

		return Highest + 1;
	}

	FVector RingOffset(int32 Index, int32 Count, float Radius)
	{
		const int32 SafeCount = FMath::Max(1, Count);
		const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(SafeCount);
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	}

	const TCHAR* RoleName(ESlimeSlotRole Role)
	{
		switch (Role)
		{
		case ESlimeSlotRole::Aggro:		return TEXT("Aggro");
		case ESlimeSlotRole::Fallback:	return TEXT("Fallback");
		default:						return TEXT("Normal");
		}
	}
}

namespace SlimeSpawnEditor
{
	USlimeRunConfig* LoadRunConfig()
	{
		return GetDefault<USlimeGameSettings>()->RunConfig.LoadSynchronous();
	}

	USlimeSpawnLayout* LoadSpawnLayout()
	{
		const USlimeRunConfig* RunConfig = LoadRunConfig();
		return RunConfig ? RunConfig->SpawnLayout.LoadSynchronous() : nullptr;
	}

	float GetPlayerMinDistance(const USlimeRunConfig* RunConfig)
	{
		return RunConfig ? RunConfig->SpawnPlayerMinDistance : 0.f;
	}

	void CollectMarkers(const ASpawnPoint& Point, TArray<USlimeSpawnSlotMarker*>& OutMarkers)
	{
		OutMarkers.Reset();

		TArray<USlimeSpawnSlotMarker*> Found;
		Point.GetComponents<USlimeSpawnSlotMarker>(Found);

		OutMarkers.Reserve(Found.Num());
		for (USlimeSpawnSlotMarker* Marker : Found)
		{
			if (Marker)
			{
				OutMarkers.Add(Marker);
			}
		}
	}

	const FSlimeSpawnPointDef* FindPointDef(const USlimeSpawnLayout& Layout, int32 PointId)
	{
		return Layout.Points.FindByPredicate(
			[PointId](const FSlimeSpawnPointDef& Def) { return Def.PointId == PointId; });
	}

	FSlimeSpawnPointDef* FindMutablePointDef(USlimeSpawnLayout& Layout, int32 PointId)
	{
		return Layout.Points.FindByPredicate(
			[PointId](const FSlimeSpawnPointDef& Def) { return Def.PointId == PointId; });
	}

	USlimeSpawnSlotMarker* AddMarker(
		ASpawnPoint& Point, ESlimeSlotRole Role, const FVector& RelativeLocation)
	{
		USceneComponent* Root = Point.GetRootComponent();
		if (!Root)
		{
			UE_LOG(LogSlimeWar, Error, TEXT("Spawn point %d has no root component."), Point.PointId);
			return nullptr;
		}

		Point.Modify();

		USlimeSpawnSlotMarker* Marker = NewObject<USlimeSpawnSlotMarker>(
			&Point, NAME_None, RF_Transactional);
		Marker->SetupAttachment(Root);
		Marker->Role = Role;
		Marker->SlotIndex = NextFreeIndex(Point, Role);
		Marker->SetRelativeLocation(RelativeLocation);

		Point.AddInstanceComponent(Marker);
		Marker->RegisterComponent();

		ApplyRoleVisuals(*Marker);
		Point.MarkPackageDirty();
		return Marker;
	}

	void RemoveMarker(ASpawnPoint& Point, USlimeSpawnSlotMarker& Marker)
	{
		Point.Modify();
		Point.RemoveInstanceComponent(&Marker);
		Marker.DestroyComponent();
		Point.MarkPackageDirty();
	}

	int32 RemoveAllMarkers(ASpawnPoint& Point)
	{
		TArray<USlimeSpawnSlotMarker*> Markers;
		CollectMarkers(Point, Markers);

		for (USlimeSpawnSlotMarker* Marker : Markers)
		{
			RemoveMarker(Point, *Marker);
		}

		return Markers.Num();
	}

	int32 CreateDefaultSlots(ASpawnPoint& Point, const USlimeRunConfig& RunConfig)
	{
		const int32 NormalCount = FMath::Max(1, RunConfig.SpawnNormalPerBatch);
		const int32 AggroCount = FMath::Max(1, RunConfig.SpawnAggroPerBatch);

		for (int32 Index = 0; Index < NormalCount; ++Index)
		{
			AddMarker(Point, ESlimeSlotRole::Normal, RingOffset(Index, NormalCount, DefaultNormalRingRadius));
		}

		for (int32 Index = 0; Index < AggroCount; ++Index)
		{
			AddMarker(Point, ESlimeSlotRole::Aggro, RingOffset(Index, AggroCount, DefaultAggroRingRadius));
		}

		for (int32 Index = 0; Index < DefaultFallbackCount; ++Index)
		{
			AddMarker(Point, ESlimeSlotRole::Fallback,
				RingOffset(Index, DefaultFallbackCount, DefaultFallbackRingRadius));
		}

		return NormalCount + AggroCount + DefaultFallbackCount;
	}

	int32 SnapMarkersToGround(ASpawnPoint& Point, float PlayerMinDistance)
	{
		const UWorld* World = Point.GetWorld();
		if (!World)
		{
			return 0;
		}

		TArray<USlimeSpawnSlotMarker*> Markers;
		CollectMarkers(Point, Markers);

		const FTransform AnchorTransform = Point.GetActorTransform();
		int32 Snapped = 0;

		for (USlimeSpawnSlotMarker* Marker : Markers)
		{
			if (!Marker)
			{
				continue;
			}

			const FVector DesiredWorld = AnchorTransform.TransformPosition(Marker->GetRelativeLocation());
			const SlimeSlotResolver::FResolution Resolution =
				SlimeSlotResolver::Resolve(*World, &Point, DesiredWorld, PlayerMinDistance);

			if (!Resolution.bValid)
			{
				UE_LOG(LogSlimeWar, Warning,
					TEXT("SnapSlotsToGround: %s could not be snapped - %s."),
					*Marker->GetName(), SlimeSlotResolver::RejectReasonName(Resolution.Reason));
				continue;
			}

			const FVector SnappedWorld(DesiredWorld.X, DesiredWorld.Y, Resolution.GroundZ);
			Marker->Modify();
			Marker->SetRelativeLocation(AnchorTransform.InverseTransformPosition(SnappedWorld));
			++Snapped;
		}

		if (Snapped > 0)
		{
			Point.MarkPackageDirty();
		}

		return Snapped;
	}

	int32 ValidatePoint(const ASpawnPoint& Point, float PlayerMinDistance)
	{
		const UWorld* World = Point.GetWorld();
		if (!World)
		{
			return 0;
		}

		TArray<USlimeSpawnSlotMarker*> Markers;
		CollectMarkers(Point, Markers);

		if (Markers.Num() == 0)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("ValidateLayout: spawn point %d has no slot handles in the level."), Point.PointId);
			return 0;
		}

		const FTransform AnchorTransform = Point.GetActorTransform();
		int32 Bad = 0;

		for (const USlimeSpawnSlotMarker* Marker : Markers)
		{
			if (!Marker)
			{
				continue;
			}

			const FVector DesiredWorld = AnchorTransform.TransformPosition(Marker->GetRelativeLocation());
			const SlimeSlotResolver::FResolution Resolution =
				SlimeSlotResolver::Resolve(*World, &Point, DesiredWorld, PlayerMinDistance);

			if (Resolution.bValid)
			{
				UE_LOG(LogSlimeWar, Log,
					TEXT("  %s %d OK   world %s   ground Z %.0f   player %.0f cm"),
					RoleName(Marker->Role),
					Marker->SlotIndex,
					*DesiredWorld.ToCompactString(),
					Resolution.GroundZ,
					Resolution.PlayerDistance);
			}
			else
			{
				++Bad;
				UE_LOG(LogSlimeWar, Warning,
					TEXT("  %s %d BAD  world %s - %s"),
					RoleName(Marker->Role),
					Marker->SlotIndex,
					*DesiredWorld.ToCompactString(),
					SlimeSlotResolver::RejectReasonName(Resolution.Reason));
			}
		}

		UE_LOG(LogSlimeWar, Log,
			TEXT("ValidateLayout: point %d - %d slot(s), %d unusable."), Point.PointId, Markers.Num(), Bad);

		return Bad;
	}

	bool IsPointInSync(
		const ASpawnPoint& Point,
		const USlimeSpawnLayout& Layout,
		int32& OutMarkerCount,
		int32& OutDefCount)
	{
		TArray<USlimeSpawnSlotMarker*> Markers;
		CollectMarkers(Point, Markers);

		OutMarkerCount = Markers.Num();
		OutDefCount = 0;

		const FSlimeSpawnPointDef* Def = FindPointDef(Layout, Point.PointId);
		if (!Def)
		{
			return OutMarkerCount == 0;
		}

		OutDefCount = Def->NormalSlots.Num() + Def->AggroSlots.Num() + Def->FallbackSlots.Num();

		TArray<SlimeSpawnLayoutEdit::FSlotEntry> Entries;
		Entries.Reserve(Markers.Num());
		for (const USlimeSpawnSlotMarker* Marker : Markers)
		{
			SlimeSpawnLayoutEdit::FSlotEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Role = Marker->Role;
			Entry.Index = Marker->SlotIndex;
			Entry.RelativeLocation = Marker->GetRelativeLocation();
			Entry.RelativeRotation = Marker->GetRelativeRotation();
		}

		TArray<FSlimeSpawnSlot> NormalSlots;
		TArray<FSlimeSpawnSlot> AggroSlots;
		TArray<FSlimeSpawnSlot> FallbackSlots;
		SlimeSpawnLayoutEdit::BuildSlotArrays(Entries, NormalSlots, AggroSlots, FallbackSlots);

		return SlotsMatch(NormalSlots, Def->NormalSlots)
			&& SlotsMatch(AggroSlots, Def->AggroSlots)
			&& SlotsMatch(FallbackSlots, Def->FallbackSlots);
	}

	void ExportPoint(ASpawnPoint& Point, USlimeSpawnLayout& Layout, FString& OutSummary)
	{
		TArray<USlimeSpawnSlotMarker*> Markers;
		CollectMarkers(Point, Markers);

		TArray<SlimeSpawnLayoutEdit::FSlotEntry> Entries;
		Entries.Reserve(Markers.Num());
		for (const USlimeSpawnSlotMarker* Marker : Markers)
		{
			SlimeSpawnLayoutEdit::FSlotEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Role = Marker->Role;
			Entry.Index = Marker->SlotIndex;
			Entry.RelativeLocation = Marker->GetRelativeLocation();
			Entry.RelativeRotation = Marker->GetRelativeRotation();
		}

		TArray<FSlimeSpawnSlot> NormalSlots;
		TArray<FSlimeSpawnSlot> AggroSlots;
		TArray<FSlimeSpawnSlot> FallbackSlots;
		SlimeSpawnLayoutEdit::BuildSlotArrays(Entries, NormalSlots, AggroSlots, FallbackSlots);

		Layout.Modify();

		FSlimeSpawnPointDef* Existing = FindMutablePointDef(Layout, Point.PointId);
		const bool bUpdated = Existing != nullptr;
		if (Existing)
		{
			// The radius is not edited in the viewport, so it must survive an export.
			Existing->NormalSlots = NormalSlots;
			Existing->AggroSlots = AggroSlots;
			Existing->FallbackSlots = FallbackSlots;
		}
		else
		{
			FSlimeSpawnPointDef& NewDef = Layout.Points.AddDefaulted_GetRef();
			NewDef.PointId = Point.PointId;
			NewDef.ActivityRadius = 0.f;
			NewDef.NormalSlots = NormalSlots;
			NewDef.AggroSlots = AggroSlots;
			NewDef.FallbackSlots = FallbackSlots;
		}

		Layout.MarkPackageDirty();

		OutSummary = FString::Printf(TEXT("%s point %d (%d normal / %d aggro / %d fallback)"),
			bUpdated ? TEXT("updated") : TEXT("added"),
			Point.PointId, NormalSlots.Num(), AggroSlots.Num(), FallbackSlots.Num());
	}

	void ExportAllPoints(UWorld& World, USlimeSpawnLayout& Layout, FString& OutSummary)
	{
		int32 Count = 0;
		TArray<int32> SeenPointIds;

		for (TActorIterator<ASpawnPoint> It(&World); It; ++It)
		{
			ASpawnPoint* Point = *It;
			if (!Point)
			{
				continue;
			}

			if (SeenPointIds.Contains(Point->PointId))
			{
				UE_LOG(LogSlimeWar, Error,
					TEXT("ExportAllPoints: duplicate PointId %d in the level - export skipped for the ")
					TEXT("second one. Point ids must be unique."), Point->PointId);
				continue;
			}

			SeenPointIds.Add(Point->PointId);

			FString PointSummary;
			ExportPoint(*Point, Layout, PointSummary);
			++Count;
		}

		OutSummary = FString::Printf(TEXT("%d point(s) exported"), Count);
	}

	void ImportPoint(ASpawnPoint& Point, const USlimeSpawnLayout& Layout, FString& OutSummary)
	{
		const FSlimeSpawnPointDef* Def = FindPointDef(Layout, Point.PointId);
		if (!Def)
		{
			OutSummary = FString::Printf(
				TEXT("no definition for point %d in DA_SpawnLayout"), Point.PointId);
			return;
		}

		const int32 Removed = RemoveAllMarkers(Point);

		TArray<SlimeSpawnLayoutEdit::FSlotEntry> Entries;
		SlimeSpawnLayoutEdit::FlattenSlotArrays(Def->NormalSlots, Def->AggroSlots, Def->FallbackSlots, Entries);

		for (const SlimeSpawnLayoutEdit::FSlotEntry& Entry : Entries)
		{
			if (USlimeSpawnSlotMarker* Marker = AddMarker(Point, Entry.Role, Entry.RelativeLocation))
			{
				Marker->SlotIndex = Entry.Index;
				Marker->SetRelativeRotation(Entry.RelativeRotation);
			}
		}

		OutSummary = FString::Printf(
			TEXT("imported %d slot(s) into point %d (replaced %d)"), Entries.Num(), Point.PointId, Removed);
	}

	FLinearColor RoleColor(ESlimeSlotRole Role)
	{
		switch (Role)
		{
		case ESlimeSlotRole::Aggro:		return FLinearColor(1.f, 0.32f, 0.28f);
		case ESlimeSlotRole::Fallback:	return FLinearColor(1.f, 0.78f, 0.25f);
		default:						return FLinearColor(0.32f, 0.86f, 0.45f);
		}
	}

	void ApplyRoleVisuals(USlimeSpawnSlotMarker& Marker)
	{
		Marker.SetArrowColor(RoleColor(Marker.Role));
	}
}
