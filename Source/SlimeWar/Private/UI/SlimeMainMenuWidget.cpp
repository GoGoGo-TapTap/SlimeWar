// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeMainMenuWidget.h"

#include "Blueprint/UserWidget.h"
#include "Core/SlimeWarLog.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/SlimeLevelSelectWidget.h"
#include "UI/SlimeSettingsWidget.h"

namespace
{
	const FName CityPlazaMap(TEXT("/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza"));
}

void USlimeMainMenuWidget::StartGame()
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (Settings && !Settings->LevelSelectWidgetClass.IsNull())
	{
		if (UClass* LevelSelectClass = Settings->LevelSelectWidgetClass.LoadSynchronous())
		{
			OpenMenuWidget(LevelSelectClass);
			return;
		}

		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeMainMenuWidget: LevelSelectWidgetClass could not be loaded (%s); opening CityPlaza."),
			*Settings->LevelSelectWidgetClass.ToString());
	}

	UGameplayStatics::OpenLevel(this, CityPlazaMap);
}

void USlimeMainMenuWidget::OpenSettings()
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Settings || Settings->SettingsWidgetClass.IsNull())
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeMainMenuWidget: SettingsWidgetClass is not configured; settings cannot open."));
		return;
	}

	if (UClass* SettingsClass = Settings->SettingsWidgetClass.LoadSynchronous())
	{
		OpenMenuWidget(SettingsClass);
		return;
	}

	UE_LOG(LogSlimeWar, Warning,
		TEXT("USlimeMainMenuWidget: SettingsWidgetClass could not be loaded (%s)."),
		*Settings->SettingsWidgetClass.ToString());
}

void USlimeMainMenuWidget::QuitGame()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UKismetSystemLibrary::QuitGame(
		this,
		PlayerController,
		EQuitPreference::Quit,
		/*bIgnorePlatformRestrictions=*/false);
}

void USlimeMainMenuWidget::OpenMenuWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeMainMenuWidget: cannot open a child menu without an owning player."));
		return;
	}

	UUserWidget* ChildWidget = CreateWidget<UUserWidget>(PlayerController, WidgetClass);
	if (!ChildWidget)
	{
		return;
	}

	if (USlimeSettingsWidget* SettingsWidget = Cast<USlimeSettingsWidget>(ChildWidget))
	{
		SettingsWidget->SetReturnWidget(this);
	}
	else if (USlimeLevelSelectWidget* LevelSelectWidget = Cast<USlimeLevelSelectWidget>(ChildWidget))
	{
		LevelSelectWidget->SetReturnWidget(this);
	}

	ChildWidget->AddToViewport(10);
	SetVisibility(ESlateVisibility::Collapsed);
	ChildWidget->SetVisibility(ESlateVisibility::Visible);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);
}
