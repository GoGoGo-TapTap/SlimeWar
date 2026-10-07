// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSpawnPointDetails.h"

#include "Core/SlimeWarLog.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/World.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "Flow/SpawnPoint.h"
#include "Fonts/SlateFontInfo.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "SlimeSpawnEditorUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Anchor-local offset given to a freshly added handle, so it does not land on an existing one. */
	FVector NextMarkerOffset(const ASpawnPoint& Point, ESlimeSlotRole Role)
	{
		TArray<USlimeSpawnSlotMarker*> Markers;
		SlimeSpawnEditor::CollectMarkers(Point, Markers);

		int32 SameRole = 0;
		for (const USlimeSpawnSlotMarker* Marker : Markers)
		{
			if (Marker && Marker->Role == Role)
			{
				++SameRole;
			}
		}

		return FVector(300.f * static_cast<float>(SameRole + 1), 0.f, 0.f);
	}

	FText MakeSyncStatusText(const ASpawnPoint* Point)
	{
		const USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout();

		if (!Layout)
		{
			return FText::FromString(TEXT("DA_SpawnLayout not set (Project Settings -> Slime War)"));
		}

		if (!Point)
		{
			return FText::FromString(TEXT("select one spawn point"));
		}

		int32 MarkerCount = 0;
		int32 DefCount = 0;
		const bool bInSync = SlimeSpawnEditor::IsPointInSync(*Point, *Layout, MarkerCount, DefCount);

		return bInSync
			? FText::FromString(FString::Printf(TEXT("in sync - %d slot(s)"), MarkerCount))
			: FText::FromString(FString::Printf(
				TEXT("OUT OF SYNC - level %d vs asset %d, press Export"), MarkerCount, DefCount));
	}

	TSharedRef<SWidget> MakeButton(const FString& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.Text(FText::FromString(Label))
			.ToolTipText(FText::FromString(Label))
			.OnClicked(OnClicked);
	}
}

TSharedRef<IDetailCustomization> FSlimeSpawnPointDetails::MakeInstance()
{
	return MakeShared<FSlimeSpawnPointDetails>();
}

void FSlimeSpawnPointDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	TWeakObjectPtr<ASpawnPoint> Point;
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (ASpawnPoint* AsPoint = Cast<ASpawnPoint>(Object.Get()))
		{
			Point = AsPoint;
			break;
		}
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		"SpawnPointTool", FText::FromString(TEXT("Spawn Point Editor")), ECategoryPriority::Important);

	// -- status --
	Category.AddCustomRow(FText::FromString(TEXT("Status")))
		.NameContent()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Level vs Data Asset")))
		]
		.ValueContent()
		[
			SNew(STextBlock).Text(TAttribute<FText>::CreateLambda(
				[Point]() { return MakeSyncStatusText(Point.Get()); }))
		];

	const auto Refresh = [&DetailBuilder]()
	{
		DetailBuilder.ForceRefreshDetails();
	};

	// -- add / create --
	Category.AddCustomRow(FText::FromString(TEXT("Add Slots")))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("Create Default Slots"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						if (const USlimeRunConfig* RunConfig = SlimeSpawnEditor::LoadRunConfig())
						{
							const int32 Created = SlimeSpawnEditor::CreateDefaultSlots(*Anchor, *RunConfig);
							UE_LOG(LogSlimeWar, Log, TEXT("CreateDefaultSlots: %d handle(s) added to point %d."),
								Created, Anchor->PointId);
						}
						else
						{
							UE_LOG(LogSlimeWar, Error,
								TEXT("CreateDefaultSlots: DA_RunConfig is not set in Project Settings."));
						}
					}

					Refresh();
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("+ Normal"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						SlimeSpawnEditor::AddMarker(*Anchor, ESlimeSlotRole::Normal,
							NextMarkerOffset(*Anchor, ESlimeSlotRole::Normal));
					}
					Refresh();
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("+ Aggro"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						SlimeSpawnEditor::AddMarker(*Anchor, ESlimeSlotRole::Aggro,
							NextMarkerOffset(*Anchor, ESlimeSlotRole::Aggro));
					}
					Refresh();
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("+ Fallback"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						SlimeSpawnEditor::AddMarker(*Anchor, ESlimeSlotRole::Fallback,
							NextMarkerOffset(*Anchor, ESlimeSlotRole::Fallback));
					}
					Refresh();
					return FReply::Handled();
				}))
			]
		];

	// -- position / validation --
	Category.AddCustomRow(FText::FromString(TEXT("Validate")))
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("Snap To Ground"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						const float MinDistance =
							SlimeSpawnEditor::GetPlayerMinDistance(SlimeSpawnEditor::LoadRunConfig());
						const int32 Snapped = SlimeSpawnEditor::SnapMarkersToGround(*Anchor, MinDistance);

						UE_LOG(LogSlimeWar, Log,
							TEXT("SnapToGround: %d handle(s) moved onto the floor (point %d)."),
							Snapped, Anchor->PointId);
					}
					Refresh();
					return FReply::Handled();
				}))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
			[
				MakeButton(TEXT("Validate Layout"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (ASpawnPoint* Anchor = Point.Get())
					{
						UE_LOG(LogSlimeWar, Log, TEXT("ValidateLayout: spawn point %d -"), Anchor->PointId);
						SlimeSpawnEditor::ValidatePoint(*Anchor,
							SlimeSpawnEditor::GetPlayerMinDistance(SlimeSpawnEditor::LoadRunConfig()));
					}
					Refresh();
					return FReply::Handled();
				}))
			]
		];

	// -- data asset round trip --
	Category.AddCustomRow(FText::FromString(TEXT("Data Asset")))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
				[
					MakeButton(TEXT("Export To Data Asset"), FOnClicked::CreateLambda([Point, Refresh]()
					{
						if (ASpawnPoint* Anchor = Point.Get())
						{
							if (USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout())
							{
								FString Summary;
								SlimeSpawnEditor::ExportPoint(*Anchor, *Layout, Summary);
								UE_LOG(LogSlimeWar, Log, TEXT("ExportToDataAsset: %s."), *Summary);
							}
							else
							{
								UE_LOG(LogSlimeWar, Error, TEXT("ExportToDataAsset: DA_SpawnLayout is not set."));
							}
						}
						Refresh();
						return FReply::Handled();
					}))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)
				[
					MakeButton(TEXT("Import From Data Asset"), FOnClicked::CreateLambda([Point, Refresh]()
					{
						if (ASpawnPoint* Anchor = Point.Get())
						{
							if (const USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout())
							{
								FString Summary;
								SlimeSpawnEditor::ImportPoint(*Anchor, *Layout, Summary);
								UE_LOG(LogSlimeWar, Log, TEXT("ImportFromDataAsset: %s."), *Summary);
							}
							else
							{
								UE_LOG(LogSlimeWar, Error, TEXT("ImportFromDataAsset: DA_SpawnLayout is not set."));
							}
						}
						Refresh();
						return FReply::Handled();
					}))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
			[
				MakeButton(TEXT("Export All Points In Level"), FOnClicked::CreateLambda([Point, Refresh]()
				{
					if (const UWorld* World = Point.IsValid() ? Point->GetWorld() : nullptr)
					{
						if (USlimeSpawnLayout* Layout = SlimeSpawnEditor::LoadSpawnLayout())
						{
							FString Summary;
							SlimeSpawnEditor::ExportAllPoints(*const_cast<UWorld*>(World), *Layout, Summary);
							UE_LOG(LogSlimeWar, Log, TEXT("ExportAllPoints: %s."), *Summary);
						}
					}
					Refresh();
					return FReply::Handled();
				}))
			]
		];
}
