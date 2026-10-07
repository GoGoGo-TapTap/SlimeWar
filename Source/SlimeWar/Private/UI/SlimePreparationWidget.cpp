// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimePreparationWidget.h"

#include "Core/SlimeWarLog.h"
#include "Flow/RunSubsystem.h"
#include "UI/SlimeUISubsystem.h"

void USlimePreparationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const URunSubsystem* Run = URunSubsystem::Get(this);
	OnPreparationReady(Run ? Run->GetDropPointCount() : 0);
}

void USlimePreparationWidget::PreviewDropPoint(int32 DropPointIndex)
{
	const URunSubsystem* Run = URunSubsystem::Get(this);
	const int32 Count = Run ? Run->GetDropPointCount() : 0;

	if (DropPointIndex < 0 || DropPointIndex >= Count)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePreparationWidget: drop point %d is out of range (%d configured)."),
			DropPointIndex, Count);
		return;
	}

	PreviewedDropPoint = DropPointIndex;
	OnDropPointPreviewed(DropPointIndex);
}

void USlimePreparationWidget::ConfirmDropPoint(int32 DropPointIndex)
{
	PreviewDropPoint(DropPointIndex);
	ConfirmPreviewedDropPoint();
}

void USlimePreparationWidget::ConfirmPreviewedDropPoint()
{
	if (PreviewedDropPoint == INDEX_NONE)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePreparationWidget: confirm without a previewed drop point, ignored."));
		return;
	}

	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->ConfirmDropPoint(PreviewedDropPoint);
	}
}
