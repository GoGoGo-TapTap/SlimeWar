// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/SlimeGameSettings.h"

#include "GameplayFramework/SlimeLevelDefinition.h"
#include "Misc/Paths.h"

const USlimeLevelDefinition* USlimeGameSettings::FindLevelByIndex(int32 Index) const
{
	if (!Levels.IsValidIndex(Index))
	{
		return nullptr;
	}

	// LoadSynchronous on a soft pointer that is already loaded is a cheap pointer read; the level
	// list is tiny and every caller needs the object, so there is nothing to gain from deferring.
	return Levels[Index].LoadSynchronous();
}

const USlimeLevelDefinition* USlimeGameSettings::FindLevelById(FName LevelId) const
{
	if (LevelId.IsNone())
	{
		return nullptr;
	}

	for (const TSoftObjectPtr<USlimeLevelDefinition>& Entry : Levels)
	{
		const USlimeLevelDefinition* Definition = Entry.LoadSynchronous();
		if (Definition && Definition->LevelId == LevelId)
		{
			return Definition;
		}
	}

	return nullptr;
}

int32 USlimeGameSettings::IndexOfLevel(FName LevelId) const
{
	if (LevelId.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const USlimeLevelDefinition* Definition = Levels[Index].LoadSynchronous();
		if (Definition && Definition->LevelId == LevelId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

const USlimeLevelDefinition* USlimeGameSettings::FindLevelByMapName(const FString& MapName) const
{
	if (MapName.IsEmpty())
	{
		return nullptr;
	}

	for (const TSoftObjectPtr<USlimeLevelDefinition>& Entry : Levels)
	{
		const USlimeLevelDefinition* Definition = Entry.LoadSynchronous();
		if (!Definition || Definition->MapAsset.IsNull())
		{
			continue;
		}

		const FString LevelMapName =
			FPaths::GetBaseFilename(Definition->MapAsset.GetLongPackageName());
		if (LevelMapName.Equals(MapName, ESearchCase::IgnoreCase))
		{
			return Definition;
		}
	}

	return nullptr;
}
