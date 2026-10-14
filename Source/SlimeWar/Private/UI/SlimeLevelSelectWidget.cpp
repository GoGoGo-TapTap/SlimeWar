// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeLevelSelectWidget.h"

#include "Core/SlimeWarLog.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName CityPlazaMap(TEXT("/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza"));
}

void USlimeLevelSelectWidget::SelectLevel(int32 LevelIndex)
{
	if (LevelIndex < 0 || LevelIndex > 2)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeLevelSelectWidget: level index %d is outside the supported range 0..2."),
			LevelIndex);
		return;
	}

	SelectedLevelIndex = LevelIndex;
	OnLevelSelectionChanged(LevelIndex, IsLevelAvailable(LevelIndex));
}

void USlimeLevelSelectWidget::EnterSelectedLevel()
{
	if (!IsLevelAvailable(SelectedLevelIndex))
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("USlimeLevelSelectWidget: level index %d is not open yet."),
			SelectedLevelIndex);
		return;
	}

	UGameplayStatics::OpenLevel(this, CityPlazaMap);
}

void USlimeLevelSelectWidget::ReturnToMainMenu()
{
	SetVisibility(ESlateVisibility::Collapsed);

	if (UUserWidget* PreviousWidget = ReturnWidget.Get())
	{
		RemoveFromParent();
		PreviousWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		RemoveFromParent();
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}
}

void USlimeLevelSelectWidget::SetReturnWidget(UUserWidget* Widget)
{
	ReturnWidget = Widget;
}

bool USlimeLevelSelectWidget::IsLevelAvailable(int32 LevelIndex) const
{
	return LevelIndex == 0;
}
