// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/SlimeHUD.h"

#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemy/SlimeEnemyBase.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/StatTableProvider.h"
#include "GameplayTagContainer.h"

namespace
{
	/** Debug-only drawing constants (not gameplay balance values). */
	constexpr float CrosshairGap = 4.f;
	constexpr float CrosshairLength = 10.f;
	constexpr float CrosshairThickness = 1.5f;
	constexpr float EnemyLabelHeight = 120.f;
	constexpr int32 ActivityCircleSegments = 24;
}

void ASlimeHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (SlimeCVars::DebugCrosshair != 0)
	{
		DrawCrosshair();
	}

	if (SlimeCVars::DebugDrawEnemyState != 0)
	{
		DrawEnemyStateDebug();
	}
}

void ASlimeHUD::DrawCrosshair()
{
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const FLinearColor Color(1.f, 1.f, 1.f, 0.85f);

	// A four-stroke cross with a gap in the middle, so the exact aim point stays readable.
	DrawLine(CenterX - CrosshairGap - CrosshairLength, CenterY, CenterX - CrosshairGap, CenterY, Color, CrosshairThickness);
	DrawLine(CenterX + CrosshairGap, CenterY, CenterX + CrosshairGap + CrosshairLength, CenterY, Color, CrosshairThickness);
	DrawLine(CenterX, CenterY - CrosshairGap - CrosshairLength, CenterX, CenterY - CrosshairGap, Color, CrosshairThickness);
	DrawLine(CenterX, CenterY + CrosshairGap, CenterX, CenterY + CrosshairGap + CrosshairLength, Color, CrosshairThickness);
}

void ASlimeHUD::DrawEnemyStateDebug()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASlimeEnemyBase> It(World); It; ++It)
	{
		const ASlimeEnemyBase* Enemy = *It;
		if (!Enemy)
		{
			continue;
		}

		const USlimeHealthComponent* Health = Enemy->GetHealthComponent();
		if (Health && Health->IsDead())
		{
			continue;
		}

		const FVector WorldLabel = Enemy->GetActorLocation() + FVector(0.f, 0.f, EnemyLabelHeight);
		const FVector Screen = Project(WorldLabel, /*bClampToZeroPlane*/ false);
		if (Screen.Z <= 0.f)
		{
			// Behind the camera.
			continue;
		}

		// --- state tags ---
		FString Label;
		if (const USlimeStateComponent* State = Enemy->GetStateComponent())
		{
			// Not named "Tags": AActor already has a member called Tags and shadowing it is an error here.
			TArray<FGameplayTag> StateTagList;
			State->GetStateTags().GetGameplayTagArray(StateTagList);

			TArray<FString> TagNames;
			TagNames.Reserve(StateTagList.Num());
			for (const FGameplayTag& Tag : StateTagList)
			{
				TagNames.Add(Tag.GetTagName().ToString());
			}

			Label = TagNames.Num() > 0 ? FString::Join(TagNames, TEXT(" | ")) : TEXT("<no state tag>");
		}

		// Keep it single line: AHUD::DrawText does not reliably render embedded newlines.
		Label += FString::Printf(TEXT("   mass %d   hp %.0f"), Enemy->GetMass(),
			Health ? Health->GetHealth() : 0.f);

		if (Enemy->GetTargetKind() == ETargetKind::Normal)
		{
			if (const ASlimeEnemyBase* Target = Enemy->GetFusionTarget())
			{
				Label += FString::Printf(TEXT("   fusion -> mass %d"), Target->GetMass());
				DrawDebugLine(World, Enemy->GetActorLocation(), Target->GetActorLocation(),
					FColor::Green, false, -1.f, 0, 2.f);
			}
			else
			{
				Label += TEXT("   no fusion target");
			}
		}

		DrawText(Label, FLinearColor::Yellow, Screen.X, Screen.Y, nullptr, 1.f);

		// --- activity radius (normal slimes only) ---
		if (Enemy->GetTargetKind() != ETargetKind::Normal)
		{
			continue;
		}

		const FVector Center = Enemy->GetActivityCenter();
		if (Center.IsNearlyZero())
		{
			continue;
		}

		// The radius is a config value; read it through the provider the same way the AI does.
		UGameInstance* GameInstance = World->GetGameInstance();
		UStatTableProvider* Provider =
			GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
		const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;
		const float Radius = RunConfig ? RunConfig->AIActivityRadius : 0.f;

		if (Radius <= 0.f)
		{
			continue;
		}

		FVector PreviousScreen = FVector::ZeroVector;
		for (int32 Segment = 0; Segment <= ActivityCircleSegments; ++Segment)
		{
			const float Angle = 2.f * PI * static_cast<float>(Segment) / ActivityCircleSegments;
			const FVector WorldPoint = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
			const FVector ScreenPoint = Project(WorldPoint, /*bClampToZeroPlane*/ false);

			if (ScreenPoint.Z > 0.f && !PreviousScreen.IsZero())
			{
				DrawLine(PreviousScreen.X, PreviousScreen.Y, ScreenPoint.X, ScreenPoint.Y,
					FLinearColor(0.2f, 0.6f, 1.f, 0.6f), 1.f);
			}

			PreviousScreen = ScreenPoint.Z > 0.f ? ScreenPoint : FVector::ZeroVector;
		}
	}
}
