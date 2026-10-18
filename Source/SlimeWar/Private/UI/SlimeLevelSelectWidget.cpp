// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeLevelSelectWidget.h"

#include "Core/SlimeWarLog.h"
#include "Flow/SlimeSessionSubsystem.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeLevelDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SlimeUISubsystem.h"

const USlimeLevelDefinition* USlimeLevelSelectWidget::ResolveLevel(int32 LevelIndex) const
{
	// The slot index is the only contract between this widget and the level list. The list itself
	// lives in DefaultGame.ini, so adding a level never touches this file.
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	return Settings ? Settings->FindLevelByIndex(LevelIndex) : nullptr;
}

int32 USlimeLevelSelectWidget::GetConfiguredLevelCount() const
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	return Settings ? Settings->Levels.Num() : 0;
}

FName USlimeLevelSelectWidget::GetLevelId(int32 LevelIndex) const
{
	const USlimeLevelDefinition* Level = ResolveLevel(LevelIndex);
	return Level ? Level->LevelId : NAME_None;
}

FText USlimeLevelSelectWidget::GetLevelDisplayName(int32 LevelIndex) const
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (Settings && Settings->Levels.Num() == 0)
	{
		// Migration fallback: slot 0 is usable, so it must not read as an empty button.
		return LevelIndex == 0
			? NSLOCTEXT("SlimeWar", "LevelSelectCurrentMap", "Current map")
			: FText::GetEmpty();
	}

	const USlimeLevelDefinition* Level = ResolveLevel(LevelIndex);
	return Level ? Level->DisplayName : FText::GetEmpty();
}

void USlimeLevelSelectWidget::SelectLevel(int32 LevelIndex)
{
	if (!IsValidLevelIndex(LevelIndex))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeLevelSelectWidget: level index %d is outside the supported range 0..%d."),
			LevelIndex, NumLevels - 1);
		return;
	}

	SelectedLevelIndex = LevelIndex;
	OnLevelSelectionChanged(LevelIndex, IsLevelAvailable(LevelIndex));
}

void USlimeLevelSelectWidget::EnterSelectedLevel()
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();

	USlimeSessionSubsystem* Session = USlimeSessionSubsystem::Get(this);
	if (!Session)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeLevelSelectWidget: no session subsystem; refusing to enter the selected level."));
		return;
	}

	// Migration fallback: with no level configured, slot 0 means "reload the map we are already in",
	// which is what this picker did before levels became data. Without it this build would ship a
	// level select where every slot is locked and the game is unreachable.
	if (!Settings || Settings->Levels.Num() == 0)
	{
		if (SelectedLevelIndex != 0)
		{
			return;
		}

		const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
		if (CurrentLevel.IsEmpty())
		{
			UE_LOG(LogSlimeWar, Error,
				TEXT("USlimeLevelSelectWidget: no level configured and the current map name is unknown."));
			return;
		}

		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimeLevelSelectWidget: no levels are configured; slot 0 reloads '%s'. ")
			TEXT("Add DA_Level_* assets to USlimeGameSettings::Levels to enable the picker."),
			*CurrentLevel);

		Session->RequestEnterGameplay();
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevel));
		return;
	}

	const USlimeLevelDefinition* Level = ResolveLevel(SelectedLevelIndex);
	if (!Level)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("USlimeLevelSelectWidget: level slot %d is not configured."),
			SelectedLevelIndex);
		return;
	}

	if (Level->MapAsset.IsNull())
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeLevelSelectWidget: level %s has no MapAsset."),
			*Level->LevelId.ToString());
		return;
	}

	const FString MapPackage = Level->MapAsset.GetLongPackageName();
	if (MapPackage.IsEmpty())
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeLevelSelectWidget: level %s has an unresolvable map path."),
			*Level->LevelId.ToString());
		return;
	}

	// Identity, then the intent flag, then the load. Everything that can fail was checked above, so
	// a bad path cannot leave "enter gameplay" pending - that flag would otherwise make the *next*
	// reload skip the main menu without any visible reason.
	Session->SetSelectedLevelId(Level->LevelId);
	Session->RequestEnterGameplay();

	UE_LOG(LogSlimeWar, Log,
		TEXT("USlimeLevelSelectWidget: entering level %s in slot %d (%s)."),
		*Level->LevelId.ToString(), SelectedLevelIndex, *MapPackage);

	UGameplayStatics::OpenLevel(this, FName(*MapPackage));
}

void USlimeLevelSelectWidget::ReturnToMainMenu()
{
	USlimeUISubsystem* UI = USlimeUISubsystem::Get(this);
	if (!UI)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("USlimeLevelSelectWidget: UI subsystem is unavailable; cannot return to the main menu."));
		return;
	}

	// Screen switch only. This page is always opened from the main menu, so going back one screen
	// must not reload the level - that is USlimeUISubsystem::ReturnToMainMenu's job, and it is only
	// meant for leaving a run.
	UI->ShowMainMenu();
}

bool USlimeLevelSelectWidget::IsLevelAvailable(int32 LevelIndex) const
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (Settings && Settings->Levels.Num() == 0)
	{
		// Same migration fallback as EnterSelectedLevel: an empty level list keeps slot 0 usable
		// instead of locking the player out of the game entirely.
		return LevelIndex == 0;
	}

	const USlimeLevelDefinition* Level = ResolveLevel(LevelIndex);
	return Level != nullptr && !Level->MapAsset.IsNull();
}
