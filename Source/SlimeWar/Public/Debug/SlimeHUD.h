// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SlimeHUD.generated.h"

/**
 * Optional C++ debug HUD, all drawn in C++ so it needs no widget assets. Every draw path is off
 * by default; the shipped HUD is the UMG widget from USlimeGameSettings::HUDWidgetClass.
 *
 *  - Slime.Debug.Crosshair 0/1        C++ placeholder crosshair
 *  - Slime.Debug.DrawEcologyState 0/1 ecology behavior, body state and core state
 *  - Slime.Debug.DrawRun 0/1          score / time / faction sites / mineral ledger
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
	void DrawEcologyStateDebug();
	void DrawWeaponDebug();
	void DrawEcologyResourceDebug();

	/** Score, countdown and faction-site summary. */
	void DrawRunDebug();
};
