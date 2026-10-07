// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SlimePresentationDirector.generated.h"

class ALevelSequenceActor;
class ULevelSequence;
class ULevelSequencePlayer;

/**
 * Plays the two Phase D cinematics (PD-07 deployment, PD-11 settlement).
 *
 * The Level Sequence is presentation only: the DEPLOYMENT is ended by `DeployDuration` from
 * DA_RunConfig, not by the sequence's own length, exactly like every other number in this project.
 * That also means a missing or mis-timed sequence degrades instead of blocking the run - with no
 * sequence configured the deployment simply confirms itself immediately.
 *
 * The settlement orbit is started here but not ended here: URunSubsystem owns `ResultOrbitDuration`
 * and moves the phase to Ended, which is when the settlement screen appears.
 */
UCLASS()
class SLIMEWAR_API USlimePresentationDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USlimePresentationDirector* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** True while a cinematic owns the camera (used by the debug HUD / tests). */
	UFUNCTION(BlueprintPure, Category = "Slime|Presentation")
	bool IsCinematicPlaying() const { return bPlaying; }

protected:
	/** Deferred one tick so the GameState (and its phase broadcasts) exist. */
	void Setup();

	void HandleRunStateChanged(ESlimeRunState NewState);

	void PlayDeployment();
	void PlayResult();

	/** Play a sequence, replacing whatever was playing. Returns false when it could not start. */
	bool StartSequence(ULevelSequence* Sequence);
	void StopCurrentSequence();

	/** Deployment is over: hand control over and blend back to the player camera. */
	void FinishDeployment();

	void BlendBackToPlayer();

	/** Presentation-only check so a sequence that drifted away from the data value is noticed. */
	void WarnIfLengthDiffers(const ULevelSequence* Sequence, float ExpectedSeconds, const TCHAR* DataName) const;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	FTimerHandle DeployTimerHandle;

	/** Guards against a second deployment timer / a stray ConfirmDeployment. */
	bool bDeploymentPending = false;
	bool bPlaying = false;
};
