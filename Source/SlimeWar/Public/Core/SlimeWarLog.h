// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Project wide log category. Replaces the template LogTemplateCharacter.
 * Exported so the editor tooling module logs into the same channel.
 */
SLIMEWAR_API DECLARE_LOG_CATEGORY_EXTERN(LogSlimeWar, Log, All);
