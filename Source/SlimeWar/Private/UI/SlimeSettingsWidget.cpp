// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeSettingsWidget.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

void USlimeSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		PendingResolution = UserSettings->GetScreenResolution();
		PendingFullscreenMode = UserSettings->GetFullscreenMode();
	}

	float SavedMasterVolume = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(
			TEXT("/Script/SlimeWar.SlimeSettingsWidget"),
			TEXT("MasterVolume"),
			SavedMasterVolume,
			GGameUserSettingsIni);
	}
	PendingMasterVolume = FMath::Clamp(SavedMasterVolume, 0.0f, 1.0f);
	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::SetMasterVolume(float Volume)
{
	PendingMasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::SetResolution(int32 Width, int32 Height)
{
	if (Width > 0 && Height > 0)
	{
		PendingResolution = FIntPoint(Width, Height);
	}

	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::SetFullscreenMode(int32 Mode)
{
	const int32 ClampedMode = FMath::Clamp(Mode, 0, 2);
	switch (ClampedMode)
	{
	case 0:
		PendingFullscreenMode = EWindowMode::Windowed;
		break;
	case 1:
		PendingFullscreenMode = EWindowMode::Fullscreen;
		break;
	case 2:
	default:
		PendingFullscreenMode = EWindowMode::WindowedFullscreen;
		break;
	}
	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::ApplySettings()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (UserSettings)
	{
		UserSettings->SetScreenResolution(PendingResolution);
		UserSettings->SetFullscreenMode(PendingFullscreenMode);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}

	if (GConfig)
	{
		GConfig->SetFloat(
			TEXT("/Script/SlimeWar.SlimeSettingsWidget"),
			TEXT("MasterVolume"),
			PendingMasterVolume,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::ResetSettings()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (UserSettings)
	{
		UserSettings->SetToDefaults();
		PendingResolution = UserSettings->GetScreenResolution();
		PendingFullscreenMode = UserSettings->GetFullscreenMode();
	}

	PendingMasterVolume = 1.0f;
	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::ReturnToPreviousMenu()
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

void USlimeSettingsWidget::SetReturnWidget(UUserWidget* Widget)
{
	ReturnWidget = Widget;
}

int32 USlimeSettingsWidget::GetFullscreenMode() const
{
	switch (PendingFullscreenMode)
	{
	case EWindowMode::Windowed:
		return 0;
	case EWindowMode::Fullscreen:
		return 1;
	case EWindowMode::WindowedFullscreen:
	default:
		return 2;
	}
}

void USlimeSettingsWidget::GetSupportedResolutions(TArray<FIntPoint>& OutResolutions) const
{
	OutResolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(OutResolutions);

	if (PendingResolution.X > 0 && PendingResolution.Y > 0 && !OutResolutions.Contains(PendingResolution))
	{
		OutResolutions.Add(PendingResolution);
	}

	OutResolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X * A.Y < B.X * B.Y;
	});
}

void USlimeSettingsWidget::ApplyMasterVolumeToAudio()
{
	UE_LOG(LogSlimeWar, Verbose,
		TEXT("USlimeSettingsWidget: master volume %.2f is staged only; Wwise RTPC integration is pending."),
		PendingMasterVolume);
}
