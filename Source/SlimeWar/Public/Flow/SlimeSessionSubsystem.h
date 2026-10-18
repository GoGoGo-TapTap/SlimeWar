// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlimeSessionSubsystem.generated.h"

class USlimeLevelDefinition;

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

	// -- Level identity (survives OpenLevel exactly like the drop point does) --

	/** Remember which level the next world belongs to. */
	void SetSelectedLevelId(FName InLevelId) { SelectedLevelId = InLevelId; }

	UFUNCTION(BlueprintPure, Category = "Slime|Run")
	FName GetSelectedLevelId() const { return SelectedLevelId; }

	/** Back to "no level chosen". Used when returning to the main menu. */
	void ClearSelectedLevelId() { SelectedLevelId = NAME_None; }

	/**
	 * Resolve the current level definition from the id.
	 * @return nullptr when nothing was selected (PIE opened a map directly) or the id no longer
	 *         exists in the level list. Callers fall back to the global settings in that case.
	 */
	const USlimeLevelDefinition* GetSelectedLevelDefinition() const;

	/** The next world must show the main menu (leaving a run). Beats the editor PIE skip. */
	void RequestMainMenu() { bMainMenuPending = true; }

	bool HasMainMenuRequest() const { return bMainMenuPending; }

	/** Read-and-clear, consumed exactly once by the UI subsystem that comes up after the reload. */
	bool ConsumeMainMenuRequest();

	/** Retry path: the next world should skip the preparation screen and deploy immediately. */
	void RequestAutoDeploy() { bAutoDeployPending = true; }

	/**
	 * The next world must enter the game instead of showing the main menu.
	 *
	 * Two callers: level selection (enter the chosen map) and "reselect drop point" (the level is
	 * reloaded and the preparation screen must come back). Without this flag both cases are
	 * indistinguishable from a fresh launch, which is why the UI would fall back to the main menu.
	 */
	void RequestEnterGameplay() { bEnterGameplayPending = true; }

	/** Returns whether the next world was entered through level selection. */
	bool HasGameplayEntryRequest() const { return bEnterGameplayPending; }

	/** Read-and-clear the level-select entry request. */
	bool ConsumeGameplayEntryRequest();

	/** Returns whether the next world is a same-level retry. */
	bool HasAutoDeployRequest() const { return bAutoDeployPending; }

	/** Read-and-clear: consumed exactly once by the URunSubsystem that comes up after the reload. */
	bool ConsumeAutoDeployRequest();

private:
	int32 SelectedDropPoint = INDEX_NONE;

	FName SelectedLevelId;

	bool bAutoDeployPending = false;
	bool bEnterGameplayPending = false;
	bool bMainMenuPending = false;
};
