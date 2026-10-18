// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimePreparationWidget.h"

#include "Core/SlimeWarLog.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeSessionSubsystem.h"
#include "UI/SlimeUISubsystem.h"

void USlimePreparationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const URunSubsystem* Run = URunSubsystem::Get(this);
	CachedDropPointCount = Run ? Run->GetDropPointCount() : 0;

	if (const USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this))
	{
		CachedLevelId = Session->GetSelectedLevelId();
	}

	OnPreparationReady(CachedDropPointCount);
}

void USlimePreparationWidget::PreviewDropPoint(int32 DropPointIndex)
{
	// Range check against the cached count: this runs on hover, and re-resolving the level's data
	// asset every time the mouse moves is pure waste.
	if (!IsValidDropPoint(DropPointIndex))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePreparationWidget: drop point %d is out of range (%d configured for level '%s')."),
			DropPointIndex, CachedDropPointCount, *CachedLevelId.ToString());
		return;
	}

	PreviewedDropPoint = DropPointIndex;
	OnDropPointPreviewed(DropPointIndex);
}

void USlimePreparationWidget::ConfirmDropPoint(int32 DropPointIndex)
{
	// Check BEFORE previewing, not after. PreviewDropPoint() deliberately leaves the previous
	// selection untouched when the index is bad, so confirming blind would deploy to whatever the
	// player hovered last - e.g. a WBP with one marker too many sends slot 6 to drop point 1.
	if (!IsValidDropPoint(DropPointIndex))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePreparationWidget: confirm for out-of-range drop point %d ignored (%d configured for level '%s')."),
			DropPointIndex, CachedDropPointCount, *CachedLevelId.ToString());
		return;
	}

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

void USlimePreparationWidget::RequestBackToMainMenu()
{
	USlimeUISubsystem* UI = USlimeUISubsystem::Get(this);
	if (!UI)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimePreparationWidget: UI subsystem is unavailable; cannot go back to the main menu."));
		return;
	}

	// ReturnToMainMenu, not ShowMainMenu: this screen is shown before a run exists, so backing out
	// means abandoning the attempt - the world is rebuilt and the next launch starts clean.
	UE_LOG(LogSlimeWar, Log,
		TEXT("USlimePreparationWidget: leaving the drop point screen for level '%s'."),
		*CachedLevelId.ToString());

	UI->ReturnToMainMenu();
}
