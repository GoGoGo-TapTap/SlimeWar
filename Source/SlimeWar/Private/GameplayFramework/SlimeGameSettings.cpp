// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/SlimeGameSettings.h"

#include "GameplayFramework/SlimeLevelDefinition.h"

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
