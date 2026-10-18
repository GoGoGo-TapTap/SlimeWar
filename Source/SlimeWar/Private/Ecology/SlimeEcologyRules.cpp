// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyRules.h"

#include "Engine/GameInstance.h"
#include "GameplayFramework/StatTableProvider.h"

const USlimeEcologyRules* USlimeEcologyRules::GetFor(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStatTableProvider* Provider =
		GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	return Provider && Provider->GetEcologyRules()
		? Provider->GetEcologyRules()
		: GetDefault<USlimeEcologyRules>();
}
