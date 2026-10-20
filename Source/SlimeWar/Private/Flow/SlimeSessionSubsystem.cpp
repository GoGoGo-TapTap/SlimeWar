// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeSessionSubsystem.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeLevelDefinition.h"

USlimeSessionSubsystem* USlimeSessionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USlimeSessionSubsystem>() : nullptr;
}

void USlimeSessionSubsystem::SetSelectedDropPoint(int32 InIndex)
{
	if (InIndex < 0)
	{
		SelectedDropPoint = INDEX_NONE;
		return;
	}

	SelectedDropPoint = InIndex;
}

bool USlimeSessionSubsystem::ConsumeAutoDeployRequest()
{
	const bool bPending = bAutoDeployPending;
	bAutoDeployPending = false;

	if (bPending)
	{
		UE_LOG(LogSlimeWar, Log,
			TEXT("USlimeSessionSubsystem: retry request consumed, deploying straight to drop point %d."),
			SelectedDropPoint);
	}

	return bPending;
}

bool USlimeSessionSubsystem::ConsumeGameplayEntryRequest()
{
	const bool bPending = bEnterGameplayPending;
	bEnterGameplayPending = false;
	return bPending;
}

bool USlimeSessionSubsystem::ConsumeMainMenuRequest()
{
	const bool bPending = bMainMenuPending;
	bMainMenuPending = false;
	return bPending;
}

bool USlimeSessionSubsystem::ConsumeLevelSelectRequest()
{
	const bool bPending = bLevelSelectPending;
	bLevelSelectPending = false;
	return bPending;
}

const USlimeLevelDefinition* USlimeSessionSubsystem::GetSelectedLevelDefinition() const
{
	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	return Settings ? Settings->FindLevelById(SelectedLevelId) : nullptr;
}
