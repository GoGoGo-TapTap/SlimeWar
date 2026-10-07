// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSpawnPointVisualizer.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "Flow/SpawnPoint.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "SlimeSpawnEditorUtils.h"

namespace
{
	/** Viewport drawing constants (editor presentation only). */
	constexpr int32 CircleSegments = 64;
	constexpr float SlotDotSize = 14.f;
	constexpr float LinkThickness = 1.f;
	constexpr float LabelHeightOffset = 60.f;
}

const ASpawnPoint* FSlimeSpawnPointVisualizer::GetOwningPoint(const UActorComponent* Component)
{
	return Component ? Cast<ASpawnPoint>(Component->GetOwner()) : nullptr;
}

void FSlimeSpawnPointVisualizer::DrawCircle(
	FPrimitiveDrawInterface* PDI, const FVector& Center, float Radius, const FLinearColor& Color)
{
	FVector Previous = Center + FVector(Radius, 0.f, 0.f);

	for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
	{
		const float Angle = 2.f * PI * static_cast<float>(Segment) / static_cast<float>(CircleSegments);
		const FVector Current = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

		PDI->DrawLine(Previous, Current, Color, SDPG_World, 2.f);
		Previous = Current;
	}
}

void FSlimeSpawnPointVisualizer::DrawVisualization(
	const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const ASpawnPoint* Point = GetOwningPoint(Component);
	if (!Point || !PDI)
	{
		return;
	}

	const FVector Center = Point->GetSpawnCentre();

	// The activity radius comes from the data asset, so the circle shows exactly what the AI uses.
	const float Radius = Point->GetEffectiveActivityRadius();
	if (Radius > 0.f)
	{
		DrawCircle(PDI, Center, Radius, FLinearColor(0.25f, 0.6f, 1.f, 0.9f));
	}

	const FTransform AnchorTransform = Point->GetActorTransform();

	TArray<USlimeSpawnSlotMarker*> Markers;
	SlimeSpawnEditor::CollectMarkers(*Point, Markers);

	for (const USlimeSpawnSlotMarker* Marker : Markers)
	{
		if (!Marker)
		{
			continue;
		}

		const FVector World = AnchorTransform.TransformPosition(Marker->GetRelativeLocation());
		const FLinearColor Color = SlimeSpawnEditor::RoleColor(Marker->Role);

		PDI->DrawLine(Center, World, FLinearColor(Color.R, Color.G, Color.B, 0.35f), SDPG_World, LinkThickness);
		PDI->DrawPoint(World, Color, SlotDotSize, SDPG_Foreground);
	}
}

void FSlimeSpawnPointVisualizer::DrawVisualizationHUD(
	const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const ASpawnPoint* Point = GetOwningPoint(Component);
	if (!Point || !Viewport || !View || !Canvas || !GEngine)
	{
		return;
	}

	const FPlane Projected = View->Project(Point->GetSpawnCentre() + FVector(0.f, 0.f, LabelHeightOffset));
	if (Projected.W <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector2D ViewportSize = Viewport->GetSizeXY();
	const FVector2D ScreenPosition(
		static_cast<float>(Projected.X / Projected.W * 0.5 + 0.5) * ViewportSize.X,
		static_cast<float>(1.0 - (Projected.Y / Projected.W * 0.5 + 0.5)) * ViewportSize.Y);

	FString Label = FString::Printf(TEXT("SpawnPoint %d"), Point->PointId);

	// Sync state right where the work happens, so "why is the game using the old layout?" is
	// answered before pressing Play.
	if (const USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout())
	{
		int32 MarkerCount = 0;
		int32 DefCount = 0;
		const bool bInSync = SlimeSpawnEditor::IsPointInSync(*Point, *Layout, MarkerCount, DefCount);

		Label += bInSync
			? FString::Printf(TEXT("   in sync (%d slots)"), MarkerCount)
			: FString::Printf(TEXT("   OUT OF SYNC: level %d vs asset %d"), MarkerCount, DefCount);
	}
	else
	{
		Label += TEXT("   DA_SpawnLayout not set");
	}

	FCanvasTextItem TextItem(ScreenPosition, FText::FromString(Label),
		GEngine->GetSmallFont(), FLinearColor::White);
	Canvas->DrawItem(TextItem);
}
