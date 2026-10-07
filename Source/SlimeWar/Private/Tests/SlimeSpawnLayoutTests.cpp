// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "Core/SlimeActivityArea.h"
#include "Flow/SlimeSlotResolver.h"
#include "Flow/SlimeSpawnLayoutEdit.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Pure-logic coverage for the anchor-relative spawn layout (Phase C revision).
 *
 * The DA round trip and the anchor-local -> world conversion are the parts the editor tool and the
 * spawner must agree on, so they are pinned here instead of only being checked by hand in PIE.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeSpawnSlotArraysTest,
	"SlimeWar.Spawn.LayoutArrays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeSpawnSlotArraysTest::RunTest(const FString& Parameters)
{
	// Markers deliberately handed over out of order, and with a gap in the normal indices.
	TArray<SlimeSpawnLayoutEdit::FSlotEntry> Entries;

	SlimeSpawnLayoutEdit::FSlotEntry& Aggro = Entries.AddDefaulted_GetRef();
	Aggro.Role = ESlimeSlotRole::Aggro;
	Aggro.Index = 1;
	Aggro.RelativeLocation = FVector(200.f, 0.f, 0.f);

	SlimeSpawnLayoutEdit::FSlotEntry& Normal2 = Entries.AddDefaulted_GetRef();
	Normal2.Role = ESlimeSlotRole::Normal;
	Normal2.Index = 2;
	Normal2.RelativeLocation = FVector(300.f, 0.f, 0.f);

	SlimeSpawnLayoutEdit::FSlotEntry& Fallback = Entries.AddDefaulted_GetRef();
	Fallback.Role = ESlimeSlotRole::Fallback;
	Fallback.Index = 0;
	Fallback.RelativeLocation = FVector(-400.f, 0.f, 0.f);

	SlimeSpawnLayoutEdit::FSlotEntry& Normal0 = Entries.AddDefaulted_GetRef();
	Normal0.Role = ESlimeSlotRole::Normal;
	Normal0.Index = 0;
	Normal0.RelativeLocation = FVector(100.f, 0.f, 0.f);

	SlimeSpawnLayoutEdit::FSlotEntry& Aggro0 = Entries.AddDefaulted_GetRef();
	Aggro0.Role = ESlimeSlotRole::Aggro;
	Aggro0.Index = 0;
	Aggro0.RelativeLocation = FVector(150.f, 0.f, 0.f);

	TArray<FSlimeSpawnSlot> NormalSlots;
	TArray<FSlimeSpawnSlot> AggroSlots;
	TArray<FSlimeSpawnSlot> FallbackSlots;
	SlimeSpawnLayoutEdit::BuildSlotArrays(Entries, NormalSlots, AggroSlots, FallbackSlots);

	TestEqual(TEXT("two normal slots"), NormalSlots.Num(), 2);
	TestEqual(TEXT("two aggro slots"), AggroSlots.Num(), 2);
	TestEqual(TEXT("one fallback slot"), FallbackSlots.Num(), 1);

	// FVector components are doubles in UE5; the float overload keeps the intent obvious.
	// Sorted by Index inside each role, so the spawner walks the authored order.
	TestEqual(TEXT("normal index 0 first"), static_cast<float>(NormalSlots[0].RelativeLocation.X), 100.f);
	TestEqual(TEXT("normal index 2 second"), static_cast<float>(NormalSlots[1].RelativeLocation.X), 300.f);
	TestEqual(TEXT("aggro index 0 first"), static_cast<float>(AggroSlots[0].RelativeLocation.X), 150.f);
	TestEqual(TEXT("aggro index 1 second"), static_cast<float>(AggroSlots[1].RelativeLocation.X), 200.f);
	TestEqual(TEXT("fallback kept"), static_cast<float>(FallbackSlots[0].RelativeLocation.X), -400.f);

	// Round trip: arrays -> flat entries -> arrays must be identical.
	TArray<SlimeSpawnLayoutEdit::FSlotEntry> Flat;
	SlimeSpawnLayoutEdit::FlattenSlotArrays(NormalSlots, AggroSlots, FallbackSlots, Flat);
	TestEqual(TEXT("round trip keeps every slot"), Flat.Num(), 5);

	TArray<FSlimeSpawnSlot> NormalAgain;
	TArray<FSlimeSpawnSlot> AggroAgain;
	TArray<FSlimeSpawnSlot> FallbackAgain;
	SlimeSpawnLayoutEdit::BuildSlotArrays(Flat, NormalAgain, AggroAgain, FallbackAgain);

	TestTrue(TEXT("normal arrays round trip"),
		NormalAgain.Num() == NormalSlots.Num()
		&& NormalAgain[0].RelativeLocation.Equals(NormalSlots[0].RelativeLocation)
		&& NormalAgain[1].RelativeLocation.Equals(NormalSlots[1].RelativeLocation));
	TestTrue(TEXT("fallback arrays round trip"),
		FallbackAgain.Num() == 1 && FallbackAgain[0].RelativeLocation.Equals(FallbackSlots[0].RelativeLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeSpawnDuplicateSlotsTest,
	"SlimeWar.Spawn.DuplicateSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeSpawnDuplicateSlotsTest::RunTest(const FString& Parameters)
{
	// Three genuinely different spots. Z is deliberately ignored by the test: a slot dragged into
	// the air is still the same XY as one on the ground, so it does not make the positions distinct.
	TArray<FSlimeSpawnSlot> Distinct;
	Distinct.AddDefaulted_GetRef().RelativeLocation = FVector(0.f, 0.f, 0.f);
	Distinct.AddDefaulted_GetRef().RelativeLocation = FVector(300.f, 0.f, 0.f);
	Distinct.AddDefaulted_GetRef().RelativeLocation = FVector(0.f, 300.f, 500.f);
	TestFalse(TEXT("different X/Y is not a duplicate"), SlimeSpawnLayoutEdit::HasDuplicateXY(Distinct));

	// The classic hand-typed case: unused array elements left at the origin.
	TArray<FSlimeSpawnSlot> Stacked;
	Stacked.AddDefaulted_GetRef().RelativeLocation = FVector(0.f, 0.f, 0.f);
	Stacked.AddDefaulted_GetRef().RelativeLocation = FVector(0.f, 0.f, 0.f);
	TestTrue(TEXT("slots stacked on the anchor are flagged"), SlimeSpawnLayoutEdit::HasDuplicateXY(Stacked));

	// Same XY but clearly different Z still counts: the runtime snaps Z to the ground anyway.
	TArray<FSlimeSpawnSlot> SameXY;
	SameXY.AddDefaulted_GetRef().RelativeLocation = FVector(120.f, 40.f, 0.f);
	SameXY.AddDefaulted_GetRef().RelativeLocation = FVector(120.f, 40.f, 900.f);
	TestTrue(TEXT("same XY under different Z is still a duplicate"), SlimeSpawnLayoutEdit::HasDuplicateXY(SameXY));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeAnchorRelativeSpaceTest,
	"SlimeWar.Spawn.AnchorSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeAnchorRelativeSpaceTest::RunTest(const FString& Parameters)
{
	// Moving and rotating the anchor must move the whole layout: this is the point of storing slot
	// offsets (and not absolute positions) in the data asset.
	FTransform Anchor(FRotator(0.f, 90.f, 0.f), FVector(1000.f, 500.f, 0.f));

	const FVector Forward = Anchor.TransformPosition(FVector(200.f, 0.f, 0.f));
	TestTrue(TEXT("anchor-local +X becomes world +Y after a 90 degree yaw"), Forward.Equals(FVector(1000.f, 700.f, 0.f)));

	Anchor.SetLocation(FVector(2000.f, 500.f, 0.f));
	const FVector Moved = Anchor.TransformPosition(FVector(200.f, 0.f, 0.f));
	TestTrue(TEXT("moving the anchor moves the slot by the same amount"), Moved.Equals(FVector(2000.f, 700.f, 0.f)));

	// Spawn Z: feet on the ground, so the actor origin sits one capsule half height above it.
	TestEqual(TEXT("ground 0 + half height 96"), SlimeSlotResolver::GetSpawnZ(0.f, 96.f), 98.f);
	TestEqual(TEXT("ground 250 + half height 96"), SlimeSlotResolver::GetSpawnZ(250.f, 96.f), 348.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeActivityAreaTest,
	"SlimeWar.Enemy.ActivityArea",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeActivityAreaTest::RunTest(const FString& Parameters)
{
	const FVector Center(1000.f, 500.f, 0.f);
	const float Radius = 600.f;

	// Design 5.1 / 5.4: a normal slime only ever acts inside its own point's radius.
	TestTrue(TEXT("the centre is inside"), SlimeActivityArea::IsInside(Center, Center, Radius));
	TestTrue(TEXT("exactly on the rim is inside"),
		SlimeActivityArea::IsInside(FVector(1000.f, 1100.f, 0.f), Center, Radius));
	TestFalse(TEXT("just past the rim is outside"),
		SlimeActivityArea::IsInside(FVector(1000.f, 1100.5f, 0.f), Center, Radius));

	// Height must not matter: the rule is about staying in the point, not about the terrain.
	TestTrue(TEXT("height is ignored"),
		SlimeActivityArea::IsInside(FVector(1000.f, 900.f, 3000.f), Center, Radius));

	// Clamping a drifted slime pulls it back onto the rim, keeping its height.
	const FVector Outside(2000.f, 500.f, 120.f);
	const FVector Clamped = SlimeActivityArea::Clamp(Outside, Center, Radius);
	TestTrue(TEXT("clamped onto the rim"), Clamped.Equals(FVector(1600.f, 500.f, 120.f), 0.1f));
	TestTrue(TEXT("clamped point is inside by definition"),
		SlimeActivityArea::IsInside(Clamped, Center, Radius, /*SlackCm=*/0.1f));

	// An inside position is left alone.
	const FVector Inside(1100.f, 500.f, 40.f);
	TestTrue(TEXT("an inside position is untouched"),
		SlimeActivityArea::Clamp(Inside, Center, Radius).Equals(Inside));

	// Unset radius / unset centre means "no constraint" (cheat spawns, hand-placed actors).
	TestTrue(TEXT("radius 0 constrains nothing"),
		SlimeActivityArea::IsInside(FVector(90000.f, 0.f, 0.f), Center, 0.f));
	TestTrue(TEXT("an unset centre constrains nothing"),
		SlimeActivityArea::IsInside(FVector(90000.f, 0.f, 0.f), FVector::ZeroVector, Radius));
	TestTrue(TEXT("clamp is a no-op when unconstrained"),
		SlimeActivityArea::Clamp(FVector(90000.f, 0.f, 0.f), Center, 0.f)
			.Equals(FVector(90000.f, 0.f, 0.f)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
