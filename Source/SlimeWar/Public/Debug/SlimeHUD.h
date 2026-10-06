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
 *  - Slime.Debug.DrawFusion 1       fusion pairs, contact progress, meeting points, cooldowns
 *  - Slime.Debug.DrawAggroPath 1    aggro chase: planned nav path vs the trail actually walked
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
	void DrawFusionDebug();
	void DrawAggroPathDebug();

	/**
	 * Breadcrumb of where an aggro slime actually went.
	 *
	 * The nav path only shows the plan; the Detour Crowd steering that makes a slime walk around a
	 * blocker never shows up in it, so "did it route around or grind in place?" is only readable
	 * from the trail.
	 */
	struct FSlimeTrail
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<FVector> Points;
		float NextSampleTime = 0.f;
	};

	FSlimeTrail* FindOrAddTrail(AActor& Actor);
	void PruneTrails();

	TArray<FSlimeTrail> Trails;
};
