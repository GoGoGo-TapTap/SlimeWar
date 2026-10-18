// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyAIController.h"

ASlimeEcologyAIController::ASlimeEcologyAIController()
{
	// The ecology movement component drives CharacterMovement directly. No MoveTo request or
	// StateTree is needed, so the default path-following component stays dormant.
	bWantsPlayerState = false;
}
