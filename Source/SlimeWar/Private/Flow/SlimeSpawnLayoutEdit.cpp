// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeSpawnLayoutEdit.h"

namespace
{
	void AppendRole(
		const TArray<SlimeSpawnLayoutEdit::FSlotEntry>& Sorted,
		TArray<FSlimeSpawnSlot>& Out)
	{
		Out.Reset();
		Out.Reserve(Sorted.Num());

		for (const SlimeSpawnLayoutEdit::FSlotEntry& Entry : Sorted)
		{
			FSlimeSpawnSlot& Slot = Out.AddDefaulted_GetRef();
			Slot.RelativeLocation = Entry.RelativeLocation;
			Slot.RelativeRotation = Entry.RelativeRotation;
		}
	}

	/** Ascending by Index; ties keep the incoming order so the tool's listing stays stable. */
	void SortByIndex(TArray<SlimeSpawnLayoutEdit::FSlotEntry>& Entries)
	{
		Entries.StableSort([](const SlimeSpawnLayoutEdit::FSlotEntry& A, const SlimeSpawnLayoutEdit::FSlotEntry& B)
		{
			return A.Index < B.Index;
		});
	}
}

namespace SlimeSpawnLayoutEdit
{
	void BuildSlotArrays(
		const TArray<FSlotEntry>& Entries,
		TArray<FSlimeSpawnSlot>& OutNormalSlots,
		TArray<FSlimeSpawnSlot>& OutAggroSlots,
		TArray<FSlimeSpawnSlot>& OutFallbackSlots)
	{
		TArray<FSlotEntry> Normal;
		TArray<FSlotEntry> Aggro;
		TArray<FSlotEntry> Fallback;

		for (const FSlotEntry& Entry : Entries)
		{
			switch (Entry.Role)
			{
			case ESlimeSlotRole::Aggro:		Aggro.Add(Entry); break;
			case ESlimeSlotRole::Fallback:	Fallback.Add(Entry); break;
			default:						Normal.Add(Entry); break;
			}
		}

		SortByIndex(Normal);
		SortByIndex(Aggro);
		SortByIndex(Fallback);

		AppendRole(Normal, OutNormalSlots);
		AppendRole(Aggro, OutAggroSlots);
		AppendRole(Fallback, OutFallbackSlots);
	}

	void FlattenSlotArrays(
		const TArray<FSlimeSpawnSlot>& NormalSlots,
		const TArray<FSlimeSpawnSlot>& AggroSlots,
		const TArray<FSlimeSpawnSlot>& FallbackSlots,
		TArray<FSlotEntry>& OutEntries)
	{
		OutEntries.Reset();
		OutEntries.Reserve(NormalSlots.Num() + AggroSlots.Num() + FallbackSlots.Num());

		const auto Append = [&OutEntries](const TArray<FSlimeSpawnSlot>& Slots, ESlimeSlotRole Role)
		{
			for (int32 Index = 0; Index < Slots.Num(); ++Index)
			{
				FSlotEntry& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.Role = Role;
				Entry.Index = Index;
				Entry.RelativeLocation = Slots[Index].RelativeLocation;
				Entry.RelativeRotation = Slots[Index].RelativeRotation;
			}
		};

		Append(NormalSlots, ESlimeSlotRole::Normal);
		Append(AggroSlots, ESlimeSlotRole::Aggro);
		Append(FallbackSlots, ESlimeSlotRole::Fallback);
	}

	bool HasDuplicateXY(const TArray<FSlimeSpawnSlot>& Slots, float ToleranceCm)
	{
		const float ToleranceSq = ToleranceCm * ToleranceCm;

		for (int32 A = 0; A < Slots.Num(); ++A)
		{
			for (int32 B = A + 1; B < Slots.Num(); ++B)
			{
				const float DistSq = FVector::DistSquared2D(Slots[A].RelativeLocation, Slots[B].RelativeLocation);
				if (DistSq <= ToleranceSq)
				{
					return true;
				}
			}
		}

		return false;
	}
}
