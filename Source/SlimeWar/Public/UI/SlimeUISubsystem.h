// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimeUISubsystem.generated.h"

class ASlimeRunGameState;
class ASlimeWarCharacter;
class APlayerController;
class UUserWidget;

/**
 * Owns every Phase D screen: preparation, HUD, settlement and the pause menu (PD-05 ~ PD-13).
 *
 * It sits in the UI layer because that is the only layer allowed to depend on both Flow (the run
 * state it mirrors) and Player (health, ammo, the pause key). Flow and Player never learn that a
 * UI exists - the character only broadcasts "P was pressed" and this class decides what that
 * means.
 *
 * World scoped rather than local-player scoped on purpose: the screens live exactly as long as the
 * level, which is also what makes the reload-based retry (PD-04) throw them all away for free.
 */
UCLASS()
class SLIMEWAR_API USlimeUISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context is not usable. */
	static USlimeUISubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// -- Actions driven by the widgets --

	/** PD-08: the only entry into a run, reached from the preparation screen. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void ConfirmDropPoint(int32 DropPointIndex);

	/** Reload the level and deploy straight to the same drop point. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestRetry();

	/** Reload the level and show the preparation screen again. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestReselectDropPoint();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void RequestQuit();

	/** Pause menu toggle (PD-13). */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void TogglePause();

	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void Resume();

	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	bool IsPaused() const { return bPaused; }

protected:
	/** Runs one tick after world begin play, when the GameState and the pawn exist. */
	void SetupUI();

	void HandleRunStateChanged(ESlimeRunState NewState);
	void HandleRunEnded(ERunEndReason Reason);

	UFUNCTION()
	void HandlePauseRequested();

	APlayerController* GetPlayerController() const;
	ASlimeRunGameState* GetRunGameState() const;
	ASlimeWarCharacter* GetPlayerCharacter() const;

	/** Create the widgets once, then show only the one the phase calls for. */
	void ApplyPhaseToScreens(ESlimeRunState Phase);
	void EnsureWidgets();

	void SetUIOnlyInput();
	void SetGameInput();
	void SetPauseState(bool bNewPaused);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PreparationWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PauseWidget;

	TWeakObjectPtr<ASlimeRunGameState> BoundGameState;
	TWeakObjectPtr<ASlimeWarCharacter> BoundCharacter;

	bool bWidgetsCreated = false;
	bool bPaused = false;
};
