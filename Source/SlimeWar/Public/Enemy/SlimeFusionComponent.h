// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeFusionComponent.generated.h"

class ASlimeEnemyBase;
class USlimeHealthComponent;
class USlimeRunConfig;

/** Where a slime currently sits in the fusion flow. Only "engaged" states block re-pairing. */
UENUM(BlueprintType)
enum class ESlimeFusionState : uint8
{
	/** Not in a fusion and not cooling down. Free to look for a partner. */
	Idle		UMETA(DisplayName = "Idle"),
	/** Paired and walking to the meeting point. */
	Approaching	UMETA(DisplayName = "Approaching"),
	/** Paired and touching; the 0.4 s contact timer is running. */
	Contacting	UMETA(DisplayName = "Contacting"),
	/** Pair is over (fused or cancelled); waiting before looking again. */
	Cooling		UMETA(DisplayName = "Cooling")
};

/** Why a pairing ended. Drives the debug draw and the retry delay decision. */
UENUM(BlueprintType)
enum class ESlimeFusionCancelReason : uint8
{
	None			UMETA(DisplayName = "None"),
	/** The other side accepted someone else first, or is no longer a valid partner. */
	Rejected		UMETA(DisplayName = "Rejected"),
	/** The partner died (or was destroyed). No retry delay: the partner is gone. */
	PartnerLost		UMETA(DisplayName = "Partner Lost"),
	/** They touched but separated again before the contact time elapsed. */
	Separated		UMETA(DisplayName = "Separated"),
	/** They could not reach each other inside AIApproachTimeout. */
	ApproachTimeout	UMETA(DisplayName = "Approach Timeout"),
	/** Same point / mass cap / validity stopped holding while approaching. */
	ConditionsLost	UMETA(DisplayName = "Conditions Lost")
};

/** Returned by AdvanceApproach so the StateTree task knows whether to keep running. */
UENUM(BlueprintType)
enum class ESlimeFusionAdvance : uint8
{
	/** Still approaching / contacting / cooling down: keep the state running. */
	InProgress	UMETA(DisplayName = "In Progress"),
	/** The pair just finished (fused or released); the task should end its state. */
	Finished	UMETA(DisplayName = "Finished")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSlimeFusionStateChangedSignature, ESlimeFusionState, NewState, ESlimeFusionCancelReason, Reason);

/**
 * PB-09 ~ PB-14: the whole "two normal slimes become one bigger slime" flow.
 *
 * Lives on ASlimeNormal only (aggressive slimes never get one). The StateTree drives it:
 *   - PB-03 target selection calls BeginPairing (request / accept / reject handshake),
 *   - the "Hold" state's task calls AdvanceApproach every frame and owns the movement.
 * The component owns the pairing truth, the contact timer and the fusion resolution; it never
 * ticks on its own, so 180 enemies only pay while they are actually in the Hold state.
 *
 * Numbers (contact time, delays, tolerance, participant cap) all come from USlimeRunConfig.
 *
 * Future N-way fusion: ResolvePairing is written as a fold over Participants (sum of mass, of
 * current health and of max health), and FusionMaxParticipants already caps the list. Raising
 * the cap and adding an arbitration rule (or moving the handshake into a world subsystem) is
 * then a local change here; the StateTree tasks, the enemy API and IBattleDirector stay as they
 * are.
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class USlimeFusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeFusionComponent();

	// -- Handshake (the only way a pair is created) ------------------------------------

	/**
	 * Ask Candidate to fuse. Both sides reserve each other on success (mutual handshake), which
	 * is what makes "three slimes touching" resolve to exactly one pair: a slime that is already
	 * engaged rejects every other request, so it can never appear in two pairs.
	 *
	 * @return true when the pair was formed (this component is the initiator).
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Fusion")
	bool BeginPairing(ASlimeEnemyBase* Candidate);

	/**
	 * Answer a pending BeginPairing. Called on the candidate's component by BeginPairing itself.
	 * @param MeetingPoint  the shared walk-to point computed by the initiator (navmesh projected).
	 * @return true when the request was accepted.
	 */
	bool RespondToRequest(ASlimeEnemyBase* Requester, const FVector& InMeetingPoint);

	/**
	 * Drop the current pair. The reason decides whether a retry delay is started
	 * (PartnerLost releases immediately, everything else waits FusionRetryDelay).
	 */
	UFUNCTION(BlueprintCallable, Category = "Slime|Fusion")
	void CancelPairing(ESlimeFusionCancelReason Reason);

	/** Called by the owner's death path before anything else is cleared. */
	void NotifyPartnerDied();

	// -- Driven by the StateTree fusion task -------------------------------------------

	/**
	 * One step of the approach / contact / cooldown logic. Called every frame by the enemy's
	 * "Hold" task while the component is engaged.
	 */
	ESlimeFusionAdvance AdvanceApproach(float DeltaTime);

	// -- Queries -----------------------------------------------------------------------

	/** True while approaching, contacting or cooling down: blocks new pairings. */
	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	bool IsEngaged() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	bool IsPaired() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	ESlimeFusionState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	ESlimeFusionCancelReason GetLastCancelReason() const { return LastCancelReason; }

	ASlimeEnemyBase* GetPartner() const { return Partner.Get(); }

	/** Currently always 2 participants; the resolve code does not assume it. */
	const TArray<TObjectPtr<ASlimeEnemyBase>>& GetParticipants() const { return Participants; }

	/** Shared walk-to point, valid while paired. */
	const FVector& GetMeetingPoint() const { return MeetingPoint; }

	/** 0..1 contact progress for the debug draw (1 = the contact time is reached). */
	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	float GetContactAlpha() const;

	/** Seconds left of the post-fusion / retry cooldown. */
	UFUNCTION(BlueprintPure, Category = "Slime|Fusion")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	/** Distance at which the two bodies count as touching, in cm (radii sum + tolerance). */
	float GetContactDistance(const ASlimeEnemyBase* Other) const;

	/**
	 * Acceptance radius used when walking to the shared meeting point, in cm.
	 * Deliberately tight (FusionContactTolerance): with the engine default both slimes can stop
	 * short of the same point and never get inside GetContactDistance().
	 */
	float GetApproachAcceptanceRadius() const;

	/** Smallest gap to the partner since the pair formed (debug draw / diagnostics). */
	float GetClosestGap() const { return ClosestGap; }

	/** True while the owner is allowed to look for a new partner. */
	bool CanStartPairing() const;

	/** Debug / test helper: form a pair with Other without the usual "is it the best target" pass. */
	bool DebugForcePair(ASlimeEnemyBase* Other);

	UPROPERTY(BlueprintAssignable, Category = "Slime|Fusion")
	FSlimeFusionStateChangedSignature OnFusionStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ASlimeEnemyBase* GetOwnerEnemy() const;

	const USlimeRunConfig* GetRunConfig() const;
	int32 GetMassCap() const;
	int32 GetMaxParticipantsFromConfig() const;

	/** Validates everything that must hold for these two to pair (both directions). */
	bool CanPairWith(const ASlimeEnemyBase* Other) const;

	/** Sets up this side of a pair (partner, meeting point, tags, mirror, collision ignore). */
	void SetupLocalPairing(ASlimeEnemyBase* Other, const FVector& InMeetingPoint, bool bInitiator);

	/** Clears pairing data and the FusionTarget mirror, but keeps State/cooldown as they are. */
	void ClearPairData();

	/** Restores normal collision between the (former) pair. Safe to call twice. */
	void SetPairCollisionIgnore(ASlimeEnemyBase* Other, bool bIgnore) const;

	/** Start the "wait before looking again" window instead of releasing immediately. */
	void StartCooldown(ESlimeFusionCancelReason Reason, float Delay);

	/** Post-fusion: re-stat the initiator and consume the partner. */
	void ResolvePairing();

	/** The partner is gone (death or destruction): release immediately, no retry delay. */
	void HandlePartnerLost();

	/** Gave up getting close enough: cancel with the retry delay (design 4.6.2). */
	void TimeOutApproach(float Gap);

	void SetState(ESlimeFusionState NewState, ESlimeFusionCancelReason Reason);

	/** Pair partner (null while idle / cooling down / after the fusion was resolved). */
	UPROPERTY(Transient)
	TObjectPtr<ASlimeEnemyBase> Partner = nullptr;

	/** Everyone taking part in the current fusion. Phase B always has 2; see the class comment. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASlimeEnemyBase>> Participants;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	ESlimeFusionState State = ESlimeFusionState::Idle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	ESlimeFusionCancelReason LastCancelReason = ESlimeFusionCancelReason::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	FVector MeetingPoint = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	float ContactTime = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	float ApproachTime = 0.f;

	/** Smallest gap seen since the pair formed. Drives the "no progress" timer (design 4.6.2). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	float ClosestGap = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|Fusion", meta = (AllowPrivateAccess = "true"))
	float CooldownRemaining = 0.f;

	/** The initiator performs the resolution; the passive side only walks and waits. */
	bool bIsInitiator = false;

	/** True while ResolvePairing runs, so destroying the partner does not loop back into a cancel. */
	bool bResolving = false;
};
