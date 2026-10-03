// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlimeWarGameMode.h"
#include "SlimeWarCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASlimeWarGameMode::ASlimeWarGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
