// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeEcologyFusionComponent.generated.h"

class ASlimeEcologyEnemy;
class USlimeBodyInteractionComponent;
class USlimeEcologyRules;
class USlimeEcologySubsystem;
class USlimeLocomotionComponent;

UENUM(BlueprintType)
enum class ESlimeEcologyFusionState : uint8
{
	Idle,
	Seeking,
	Contacting,
	Cooldown,
	Blocked
};

UENUM(BlueprintType)
enum class ESlimeEcologyFusionPath : uint8
{
	None,
	Autonomous,
	Contact,
	Forced
};

UENUM(BlueprintType)
enum class ESlimeEcologyFusionEndReason : uint8
{
	Resolved,
	Interrupted,
	Separated,
	PartnerLost,
	Timeout
};

/** M4 ecology fusion: autonomous seek, stable contact and player displacement matching. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeEcologyFusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeEcologyFusionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ForceReady();
	bool TryBeginAutonomousFusion();
	bool DebugForcePair(ASlimeEcologyEnemy* Other);
	bool DebugForcedPair(ASlimeEcologyEnemy* Other);
	void DebugSetSuccessCooldown(float Seconds);
	void InterruptFusion(float BlockSeconds = -1.f);
        void ApplyShotFusionInterference(float CooldownSeconds = -1.f);
	void ApplySplitCalm(float Seconds);
	void ResetForAssimilation();
	void SetTemporaryContactReady(float Seconds);
	void NotifyPartnerDied();

	bool IsEngaged() const;
	bool IsPaired() const;
	bool IsContacting() const;
	ESlimeEcologyFusionState GetState() const { return State; }
	ESlimeEcologyFusionPath GetPath() const { return Path; }
	ASlimeEcologyEnemy* GetPartner() const { return Partner.Get(); }
	const FVector& GetMeetingPoint() const { return MeetingPoint; }
	float GetContactAlpha() const;
	float GetContactTime() const { return ContactTime; }
	float GetAutoReadyRemaining() const { return AutoReadyRemaining; }
	float GetCooldownRemaining() const { return CooldownRemaining; }
	float GetBlockRemaining() const { return BlockRemaining; }
        float GetShotCooldownRemaining() const { return ShotCooldownRemaining; }
	bool IsShotSnapshotReady() const { return bShotSnapshotReady; }
	float GetSplitCalmRemaining() const { return SplitCalmRemaining; }
	int32 GetFusionCount() const { return FusionCount; }

private:
	ASlimeEcologyEnemy* GetOwnerEnemy() const;
	USlimeBodyInteractionComponent* GetBody() const;
	USlimeLocomotionComponent* GetLocomotion() const;
	USlimeEcologySubsystem* GetEcologySubsystem() const;
	const USlimeEcologyRules* GetRules() const;

	bool BeginPairing(ASlimeEcologyEnemy* Other, ESlimeEcologyFusionPath InPath);
	bool AcceptPairing(ASlimeEcologyEnemy* Requester, ESlimeEcologyFusionPath InPath);
	bool CanPairWith(const ASlimeEcologyEnemy* Other, ESlimeEcologyFusionPath InPath) const;
	bool CanRunAutonomous() const;
	bool CanJoinForced(const ASlimeEcologyEnemy* Other) const;
	ASlimeEcologyEnemy* FindBestPartner(ESlimeEcologyFusionPath InPath) const;
	bool HasLineOfSight(const ASlimeEcologyEnemy* Other) const;
	float GetContactDistance(const ASlimeEcologyEnemy* Other) const;
	float GetForcedCaptureDistance(const ASlimeEcologyEnemy* Other) const;

	void SetupLocalPairing(
		ASlimeEcologyEnemy* Other,
		ESlimeEcologyFusionPath InPath,
		const FVector& InMeetingPoint,
		bool InIsInitiator);
	void ClearPairing();
	void SetPairCollisionIgnore(ASlimeEcologyEnemy* Other, bool bIgnore) const;
	void SetState(ESlimeEcologyFusionState NewState);
	void ResetAutoReady();
	float PickAutoReadyDelay() const;
	void TickPairing(float DeltaTime);
	void TickSearch(float DeltaTime);
	void TryForcedFusion();
	bool TryBeginContactFusion();
	void CancelPairing(ESlimeEcologyFusionEndReason Reason, float BlockSeconds = -1.f);
	void ResolvePairing();
	void ResolveForcedPair(ASlimeEcologyEnemy* Other);

	TWeakObjectPtr<ASlimeEcologyEnemy> Partner;
	TWeakObjectPtr<ASlimeEcologyEnemy> LastFusionPartner;
	ESlimeEcologyFusionState State = ESlimeEcologyFusionState::Idle;
	ESlimeEcologyFusionPath Path = ESlimeEcologyFusionPath::None;
	FVector MeetingPoint = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	float AutoReadyRemaining = 0.f;
	float CooldownRemaining = 0.f;
        float ShotCooldownRemaining = 0.f;
	float BlockRemaining = 0.f;
	float SplitCalmRemaining = 0.f;
	float ContactTime = 0.f;
	float SeekElapsed = 0.f;
	float SearchAccumulator = 0.f;
	float TemporaryContactReadyRemaining = 0.f;
	int32 FusionCount = 0;
	bool bInitiator = false;
	bool bResolving = false;
        bool bShotSnapshotReady = false;
};
