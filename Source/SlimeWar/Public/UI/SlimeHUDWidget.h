// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "SlimeHUDWidget.generated.h"

class ASlimeRunGameState;
class USlimeHealthComponent;
class USlimeWeaponComponent;

/**
 * In-run HUD (PD-09) plus the tutorial toasts (PD-12), all as Blueprint hooks.
 *
 * The C++ side only decides WHICH event fires and WHAT the numbers are; the WBP decides layout,
 * animation and styling, exactly like the debug HUD is C++ but the real HUD is not. Nothing here
 * ever touches GAS - the health mirror and the weapon component are the whole data surface.
 */
UCLASS(Abstract)
class SLIMEWAR_API USlimeHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Push every value once; also drives the "first landing" hint. Called when the HUD is shown. */
	UFUNCTION(BlueprintCallable, Category = "Slime|UI")
	void UpdateAll();

	// -- Blueprint hooks --

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnScoreUpdated(int32 Score, int32 TargetScore);

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnTimeUpdated(int32 RemainingSeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnPointStateUpdated(int32 PointId, ESpawnPointState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnHealthUpdated(float Health, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnAmmoUpdated(int32 CurrentAmmo, int32 MagazineSize);

	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnBatchIncoming(int32 BatchIndex, float SecondsUntilSpawn);

	/**
	 * PA-12: one kill was scored, fly a slime icon into the score readout.
	 *
	 * The kill location is not available (IBattleDirector::OnEnemyKilled carries kind and mass
	 * only, and Phase D deliberately does not change that contract), so the WBP should start the
	 * icon at the crosshair. DeltaScore is how much the last kill was worth, so the icon can show
	 * the amount; mass is not available above the score subsystem.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void OnScoreEarned(int32 NewScore, int32 DeltaScore);

	/** PD-12: one short toast. Never pauses the fight, never takes focus. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Slime|UI")
	void ShowTutorial(const FText& Message);

protected:
	void BindRun();
	void UnbindRun();
	void BindPlayer();
	void UnbindPlayer();

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float MaxHealth);

	UFUNCTION()
	void HandleAmmoChanged(int32 CurrentAmmo, int32 MagazineSize);

	void HandleScoreChanged(int32 NewScore);
	void HandleTimeChanged(int32 NewRemainingSeconds);
	void HandlePointStateChanged(int32 PointId, ESpawnPointState NewState);
	void HandleBatchIncoming(int32 BatchIndex, float SecondsUntilSpawn);
	void HandleFusionHint(FVector Location);
	void HandleRunStateChanged(ESlimeRunState NewState);
	void HandleRunEnded(ERunEndReason Reason);

	ASlimeRunGameState* GetRunGameState() const;

	/** Set (don't broadcast) the values used by the "once per run" hints. */
	void EvaluateHints();

	TWeakObjectPtr<USlimeHealthComponent> BoundHealth;
	TWeakObjectPtr<USlimeWeaponComponent> BoundWeapon;
	TWeakObjectPtr<ASlimeRunGameState> BoundGameState;

	/** Hints are once-per-run; the widget is rebuilt by the level reload between attempts. */
	bool bShownLandingHint = false;
	bool bShownReloadHint = false;
	bool bShownTargetHint = false;
	bool bShownLastSecondsHint = false;

	/** Previous score, so HandleScoreChanged can tell the WBP how much the last kill was worth. */
	int32 LastKnownScore = 0;
};
