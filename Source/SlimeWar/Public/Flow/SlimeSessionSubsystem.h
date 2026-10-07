// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeSessionSubsystem.generated.h"

/**
 * The little bit of run intent that has to outlive a level reload (Phase D, PD-04).
 *
 * Retrying and re-selecting a drop point both reload the level, which throws away every
 * UWorldSubsystem - including the run timeline. What must NOT be thrown away is the player's
 * intent ("同一落点重试" vs "重新选落点"), so that lives on the GameInstance.
 *
 * The best score already survives for the same reason: UScoreSubsystem is a GameInstanceSubsystem.
 */
UCLASS()
class SLIMEWAR_API USlimeSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context is not usable. */
	static USlimeSessionSubsystem* Get(const UObject* WorldContextObject);

	/** Remember the drop point the next run should use. */
	void SetSelectedDropPoint(int32 InIndex);

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	int32 GetSelectedDropPoint() const { return SelectedDropPoint; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	bool HasSelectedDropPoint() const { return SelectedDropPoint != INDEX_NONE; }

	/** Cleared when the preparation screen is shown again (the player is choosing a new point). */
	void ClearSelectedDropPoint() { SelectedDropPoint = INDEX_NONE; }

	/** Retry path: the next world should skip the preparation screen and deploy immediately. */
	void RequestAutoDeploy() { bAutoDeployPending = true; }

	/** Read-and-clear: consumed exactly once by the URunSubsystem that comes up after the reload. */
	bool ConsumeAutoDeployRequest();

private:
	int32 SelectedDropPoint = INDEX_NONE;

	bool bAutoDeployPending = false;
};
