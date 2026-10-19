// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWeaponRules.h"

#include "Engine/GameInstance.h"
#include "GameplayFramework/StatTableProvider.h"

const USlimeWeaponRules* USlimeWeaponRules::GetFor(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UStatTableProvider* Provider =
		GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
	return Provider && Provider->GetWeaponRules()
		? Provider->GetWeaponRules()
		: GetDefault<USlimeWeaponRules>();
}
