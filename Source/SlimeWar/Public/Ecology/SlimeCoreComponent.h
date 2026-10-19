// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/SlimeWarCoreTypes.h"
#include "SlimeCoreComponent.generated.h"

class AActor;
class ASlimeEcologyEnemy;
class USlimeBodyInteractionComponent;
class USlimeEcologyRules;
class USlimeEcologySubsystem;
class USlimeLocomotionComponent;

UENUM(BlueprintType)
enum class ESlimeCoreCombatPhase : uint8
{
	Idle,
	Waking,
	Chasing,
	Windup,
	Pouncing,
	Recovering,
	Retreating
};

/** Core ownership, hostile combat, pounce, devour and player-kill detonation. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeCoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeCoreComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void ArmCore(ESlimeCoreState InState);
	void SetCoreState(ESlimeCoreState InState) { ArmCore(InState); }
	void SetPlayerProvoked(float Seconds = 0.f);
	void ClearCombat();
	void HandleOwnerDeath();

	bool IsCore() const { return CoreState != ESlimeCoreState::None; }
	bool IsHostileCore() const { return CoreState == ESlimeCoreState::Hostile; }
	bool ShouldOverrideBrain() const;
	ESlimeCoreState GetCoreState() const { return CoreState; }
	ESlimeCoreCombatPhase GetCombatPhase() const { return Phase; }
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }
	float GetWakeRemaining() const { return WakeRemaining; }
	float GetAttackCooldownRemaining() const { return AttackCooldownRemaining; }
	float GetWindupRemaining() const { return WindupRemaining; }
	float GetPounceRemaining() const { return PounceRemaining; }
	float GetPlayerProvokedRemaining() const { return PlayerProvokedRemaining; }
	bool HasExploded() const { return bExploded; }
	const TCHAR* GetPhaseName() const;

	void ForceTargetForTest(AActor* InTarget);
	bool DebugDevour(ASlimeEcologyEnemy* Victim);

private:
	ASlimeEcologyEnemy* GetOwnerEnemy() const;
	USlimeLocomotionComponent* GetLocomotion() const;
	USlimeBodyInteractionComponent* GetBody() const;
	USlimeEcologySubsystem* GetEcologySubsystem() const;
	const USlimeEcologyRules* GetRules() const;

	void AcquireTarget();
	bool IsValidCombatTarget(const AActor* Target) const;
	bool HasLineOfSight(const AActor* Target) const;
	void StartWindup(AActor* Target);
	void StartPounce();
	void TickPounce(float DeltaTime);
	void ProcessPounceHits(const FVector& From, const FVector& To);
	void FinishPounce();
	void ApplyPounceDamage(AActor* Target);
	void Devour(ASlimeEcologyEnemy* Victim);
	void Detonate();
	bool IsPlayerCauser(const AActor* Causer) const;

	ESlimeCoreState CoreState = ESlimeCoreState::None;
	ESlimeCoreCombatPhase Phase = ESlimeCoreCombatPhase::Idle;
	TWeakObjectPtr<AActor> CombatTarget;
	FVector TargetLastSeenLocation = FVector::ZeroVector;
	FVector HomeLocation = FVector::ZeroVector;
	FVector PounceDirection = FVector::ForwardVector;
	FVector PreviousPounceLocation = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> PounceHitTargets;
	float WakeRemaining = 0.f;
	float CombatMemoryRemaining = 0.f;
	float TargetRefreshRemaining = 0.f;
	float AttackCooldownRemaining = 0.f;
	float WindupRemaining = 0.f;
	float PounceRemaining = 0.f;
	float RecoveryRemaining = 0.f;
	float RetreatRemaining = 0.f;
	float PlayerProvokedRemaining = 0.f;
	bool bExploded = false;
};
