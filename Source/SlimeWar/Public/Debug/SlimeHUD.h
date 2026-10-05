// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SlimeHUD.generated.h"

/**
 * Phase A debug HUD, all drawn in C++ so it needs no widget assets.
 *
 *  - Slime.Debug.Crosshair 1        placeholder crosshair (Phase D replaces it with the real HUD)
 *  - Slime.Debug.DrawEnemyState 1   enemy state tags / activity radius / fusion target links
 *
 * The aim assist visualisation lives on ASlimeWarCharacter (Slime.Debug.DrawAimAssist),
 * because that is where the camera and the assist query already are.
 */
UCLASS()
class ASlimeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void DrawCrosshair();
	void DrawEnemyStateDebug();
};
