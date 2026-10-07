// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeSpawnSlotMarkerDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Flow/SlimeSpawnSlotMarker.h"
#include "Flow/SpawnPoint.h"
#include "PropertyHandle.h"
#include "SlimeSpawnEditorUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FSlimeSpawnSlotMarkerDetails::MakeInstance()
{
	return MakeShared<FSlimeSpawnSlotMarkerDetails>();
}

void FSlimeSpawnSlotMarkerDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	TWeakObjectPtr<USlimeSpawnSlotMarker> Marker;
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		if (USlimeSpawnSlotMarker* AsMarker = Cast<USlimeSpawnSlotMarker>(Object.Get()))
		{
			Marker = AsMarker;
			break;
		}
	}

	// Changing the role must recolour the arrow, and that rule belongs to the editor module.
	if (Marker.IsValid())
	{
		static const FName RolePropertyName = GET_MEMBER_NAME_CHECKED(USlimeSpawnSlotMarker, Role);
		if (const TSharedPtr<IPropertyHandle> RoleHandle = DetailBuilder.GetProperty(RolePropertyName))
		{
			RoleHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([Marker]()
			{
				if (USlimeSpawnSlotMarker* LiveMarker = Marker.Get())
				{
					SlimeSpawnEditor::ApplyRoleVisuals(*LiveMarker);
				}
			}));
		}
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		"SpawnSlotTool", FText::FromString(TEXT("Spawn Slot")), ECategoryPriority::Important);

	Category.AddCustomRow(FText::FromString(TEXT("Remove")))
		.WholeRowContent()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Remove Slot")))
			.ToolTipText(FText::FromString(TEXT("Delete this spawn slot handle from the point")))
			.OnClicked(FOnClicked::CreateLambda([Marker]()
			{
				if (USlimeSpawnSlotMarker* LiveMarker = Marker.Get())
				{
					if (ASpawnPoint* Owner = Cast<ASpawnPoint>(LiveMarker->GetOwner()))
					{
						SlimeSpawnEditor::RemoveMarker(*Owner, *LiveMarker);
					}
				}

				return FReply::Handled();
			}))
		];
}
