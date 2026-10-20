// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeSettingsWidget.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/SlimeUISubsystem.h"

void USlimeSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		PendingResolution = ResolveValidResolution(UserSettings);
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
	if (!UserSettings)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeSettingsWidget::ApplySettings: no UGameUserSettings; display settings were not applied."));
	}
	else
	{
		// Never hand 0x0 to the engine: some platforms quietly substitute the desktop size, so it
		// looks fine, but 0x0 is still written to GameUserSettings.ini and the next launch starts
		// from the same broken value.
		PendingResolution = ResolveValidResolution(UserSettings);

		UserSettings->SetScreenResolution(PendingResolution);
		UserSettings->SetFullscreenMode(PendingFullscreenMode);
		UserSettings->ApplySettings(/*bCheckForCommandLineOverrides=*/false);
		UserSettings->SaveSettings();

		UE_LOG(LogSlimeWar, Log, TEXT("USlimeSettingsWidget: applied %dx%d, window mode %d."),
			PendingResolution.X, PendingResolution.Y, GetFullscreenMode());
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

		// SetToDefaults() clears the resolution to 0x0; read it back through the same helper the
		// construct path uses, or the combo box shows "0 x 0" right after a reset.
		PendingResolution = ResolveValidResolution(UserSettings);
		PendingFullscreenMode = UserSettings->GetFullscreenMode();
	}

	PendingMasterVolume = 1.0f;

	// Staged only: the window does not change until "Apply" is pressed.
	OnSettingsValuesChanged();
}

void USlimeSettingsWidget::ReturnToPreviousMenu()
{
	// Restore the caller BEFORE removing this widget, otherwise one frame has neither screen and
	// the transition reads as a flicker.
	if (UUserWidget* PreviousWidget = ReturnWidget.Get())
	{
		PreviousWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// SetReturnWidget() was never called (or the caller is gone). Returning to the main menu by
		// reloading the world would add a multi-second black screen, so switch screens instead.
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeSettingsWidget::ReturnToPreviousMenu: no return widget, falling back to ShowMainMenu()."));

		if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
		{
			UI->ShowMainMenu();
		}
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
	}

	SetVisibility(ESlateVisibility::Collapsed);
	RemoveFromParent();
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

	TArray<FIntPoint> PlatformResolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(PlatformResolutions);

	for (const FIntPoint& Resolution : PlatformResolutions)
	{
		if (Resolution.X > 0 && Resolution.Y > 0)
		{
			OutResolutions.AddUnique(Resolution);
		}
	}

	// The current value and the desktop mode are added even when the platform list omits them:
	// otherwise the combo box cannot show the resolution the game is actually running at.
	if (PendingResolution.X > 0 && PendingResolution.Y > 0)
	{
		OutResolutions.AddUnique(PendingResolution);
	}

	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const FIntPoint Desktop = UserSettings->GetDesktopResolution();
		if (Desktop.X > 0 && Desktop.Y > 0)
		{
			OutResolutions.AddUnique(Desktop);
		}
	}

	// Area, then X, then Y: the platform list is not guaranteed to be ordered, and a stable order is
	// what keeps the combo box index -> resolution mapping from shifting between sessions.
	OutResolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		const int64 AreaA = static_cast<int64>(A.X) * A.Y;
		const int64 AreaB = static_cast<int64>(B.X) * B.Y;
		if (AreaA != AreaB)
		{
			return AreaA < AreaB;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.Y < B.Y;
	});
}

FIntPoint USlimeSettingsWidget::ResolveValidResolution(const UGameUserSettings* UserSettings) const
{
	if (!UserSettings)
	{
		return FIntPoint(1280, 720);
	}

	const FIntPoint Current = UserSettings->GetScreenResolution();
	if (Current.X > 0 && Current.Y > 0)
	{
		return Current;
	}

	const FIntPoint Desktop = UserSettings->GetDesktopResolution();
	if (Desktop.X > 0 && Desktop.Y > 0)
	{
		UE_LOG(LogSlimeWar, Verbose,
			TEXT("USlimeSettingsWidget: no saved resolution, using the desktop size %dx%d."), Desktop.X, Desktop.Y);
		return Desktop;
	}

	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);

	FIntPoint Largest = FIntPoint::ZeroValue;
	int64 LargestArea = 0;
	for (const FIntPoint& Resolution : Supported)
	{
		if (Resolution.X <= 0 || Resolution.Y <= 0)
		{
			continue;
		}

		const int64 Area = static_cast<int64>(Resolution.X) * Resolution.Y;
		if (Area > LargestArea)
		{
			LargestArea = Area;
			Largest = Resolution;
		}
	}

	if (LargestArea > 0)
	{
		UE_LOG(LogSlimeWar, Verbose,
			TEXT("USlimeSettingsWidget: no saved or desktop resolution, using the largest supported %dx%d."),
			Largest.X, Largest.Y);
		return Largest;
	}

	UE_LOG(LogSlimeWar, Warning,
		TEXT("USlimeSettingsWidget: the platform reported no usable resolution; falling back to 1280x720."));
	return FIntPoint(1280, 720);
}
