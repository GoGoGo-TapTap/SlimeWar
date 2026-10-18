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

UENUM()
enum class ESlimeUIFlowState : uint8
{
	MainMenu,
	LevelSelect,
	Preparation,
	Deploying,
	Running,
	Pause,
	Result
};

/**
 * Owns every menu and Phase D screen: main menu, level select, preparation, HUD, settlement and
 * pause menu (PD-05 ~ PD-13).
 *
 * It sits in the UI layer because that is the only layer allowed to depend on both Flow (the run
 * state it mirrors) and Player (health, ammo, the pause key). Flow and Player never learn that a
 * UI exists - the character only broadcasts "Escape was pressed" and this class decides what that
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

	/** Show the level selection screen from the main menu. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void OpenLevelSelect();

	/**
	 * Show the main menu by switching screens only - the world is NOT reloaded.
	 *
	 * This is the one menu navigation uses (level select -> main menu). Reloading a whole level to
	 * go back one screen reads as a freeze, so the two operations are deliberately separate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void ShowMainMenu();

	/** Return to the main menu by reloading the current world. Use this to leave a run, not to navigate. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void ReturnToMainMenu();

	/** Pause menu toggle (PD-13). */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void TogglePause();

	/** Stable id of the level being played; empty when PIE opened a map directly. */
	UFUNCTION(BlueprintPure, Category = "Slime|UI")
	FName GetCurrentLevelId() const;

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

	/** Apply whatever phase the GameState holds now. Used by the one-tick deferral of a retry. */
	void ApplyBoundPhaseToScreens();
	void EnsureWidgets();

	/**
	 * Create the drop point screen for the current level, on demand.
	 *
	 * Idempotent. It is separate from EnsureWidgets() because the class depends on the level, and
	 * because the menu world never needs it.
	 */
	void EnsurePreparationWidget();

	/** Shared loader: soft class -> widget at viewport Z 0, hidden. Null when the class is missing. */
	UUserWidget* CreateScreenWidget(const TSoftClassPtr<UUserWidget>& WidgetClass, const FString& Label);

	void ApplyMenuState(ESlimeUIFlowState NewState);
	bool ShouldEnterGameplay() const;

	void SetUIOnlyInput();
	void SetGameInput();
	void SetPauseState(bool bNewPaused);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LevelSelectWidget;

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
	ESlimeUIFlowState UIFlowState = ESlimeUIFlowState::MainMenu;
};
