// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyRules.h"

#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FLinearColor FactionColorFallback(ESlimeFaction Faction)
	{
		switch (Faction)
		{
		case ESlimeFaction::A:
			return FLinearColor(0.518f, 0.863f, 0.996f, 1.f);
		case ESlimeFaction::B:
			return FLinearColor(0.953f, 0.729f, 0.769f, 1.f);
		case ESlimeFaction::C:
			return FLinearColor(1.f, 0.953f, 0.737f, 1.f);
		default:
			return FLinearColor::White;
		}
	}
}

USlimeEcologyRules::USlimeEcologyRules()
{
	// Useful defaults so a fresh rules asset still gets the v2 faction look.
	// Designers can replace the body meshes and materials per faction in DA_EcologyRules.
	const TSoftObjectPtr<UStaticMesh> DefaultBody(
		FSoftObjectPath(TEXT("/Game/Characters/Slime/SM_Slime_Base.SM_Slime_Base")));

	FSlimeFactionVisual FactionA;
	FactionA.BodyMesh = DefaultBody;
	FactionA.BodyMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionA.MI_Slime_FactionA")));
	FactionA.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal_A.M_Slime_Splatter_Decal_A")));
	FactionVisuals.Add(ESlimeFaction::A, FactionA);

	FSlimeFactionVisual FactionB;
	FactionB.BodyMesh = DefaultBody;
	FactionB.BodyMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionB.MI_Slime_FactionB")));
	FactionB.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal_B.M_Slime_Splatter_Decal_B")));
	FactionVisuals.Add(ESlimeFaction::B, FactionB);

	FSlimeFactionVisual FactionC;
	FactionC.BodyMesh = DefaultBody;
	FactionC.BodyMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/MI_Slime_FactionC.MI_Slime_FactionC")));
	FactionC.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal_C.M_Slime_Splatter_Decal_C")));
	FactionVisuals.Add(ESlimeFaction::C, FactionC);

	FactionColors.Add(ESlimeFaction::A, FactionColorFallback(ESlimeFaction::A));
	FactionColors.Add(ESlimeFaction::B, FactionColorFallback(ESlimeFaction::B));
	FactionColors.Add(ESlimeFaction::C, FactionColorFallback(ESlimeFaction::C));

	SpikeMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/SM_Slime_Spike.SM_Slime_Spike")));
	CoreMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/SM_Slime_Core.SM_Slime_Core")));
	WeakpointMesh = TSoftObjectPtr<UStaticMesh>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/SM_Slime_Weakpoint.SM_Slime_Weakpoint")));
	CoreMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/MI_Slime_Core.MI_Slime_Core")));
	WeakpointMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/MI_Slime_Weakpoint.MI_Slime_Weakpoint")));
	PaintDecalMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal_A.M_Slime_Splatter_Decal_A")));
}

const USlimeEcologyRules* USlimeEcologyRules::GetFor(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStatTableProvider* Provider =
		GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	return Provider && Provider->GetEcologyRules()
		? Provider->GetEcologyRules()
		: GetDefault<USlimeEcologyRules>();
}

const FSlimeFactionVisual* USlimeEcologyRules::FindFactionVisual(ESlimeFaction Faction) const
{
	return FactionVisuals.Find(Faction);
}

FLinearColor USlimeEcologyRules::GetFactionColorOrDefault(ESlimeFaction Faction) const
{
	if (const FLinearColor* Color = FactionColors.Find(Faction))
	{
		return *Color;
	}

	return FactionColorFallback(Faction);
}
