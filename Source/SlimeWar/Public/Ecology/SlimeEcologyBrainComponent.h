// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeEcologyBrainComponent.generated.h"

class APawn;
class ASlimeEcologyEnemy;
class ASlimeCargoPickup;
class ASlimeMineralPatch;
class USlimeEcologyRules;
class USlimeEcologySubsystem;
class USlimeLocomotionComponent;
class USlimeAlertComponent;

UENUM(BlueprintType)
enum class ESlimeEcologyBehavior : uint8
{
	Paused,
	CoreCombat,
	AlertWait,
	AvoidHostileCore,
	AlertAvoidEnemy,
	AlertRegroup,
	AvoidPlayer,
	Regroup,
	Explore,
	FusionSeek,
	RecoverCargo,
	ReturnCargo,
	Gather,
	SeekPaint
};

/** M2 perception, group and exploration decisions for one ecology slime. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeEcologyBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeEcologyBrainComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void SetHome(const FVector& InHome, float InActivityRadius);
	void ForceRefresh();
	void ClearResourceMemory();
	void DebugSetGatherRest(float Seconds);

	ESlimeEcologyBehavior GetBehavior() const { return Behavior; }
	const TCHAR* GetBehaviorName() const;
	FVector GetGroupCenter() const { return GroupCenter; }
	FVector GetDesiredGoal() const { return DesiredGoal; }
	float GetDesiredSpeed() const { return DesiredSpeed; }
	int32 GetVisibleAllyCount() const;
	int32 GetRefreshCount() const { return RefreshCount; }
	bool IsPlayerVisible() const { return bPlayerVisible; }
	float GetPlayerDistance() const { return PlayerDistance; }
	float GetGatherRestRemaining() const { return GatherRestRemaining; }
	int32 GetExplorationVisitCount() const { return ExplorationVisits.Num(); }

	/** Cached-neighbour separation force used by the per-frame locomotion layer. */
	FVector GetSeparationVector() const;

private:
	ASlimeEcologyEnemy* GetOwnerEnemy() const;
	USlimeLocomotionComponent* GetLocomotion() const;
	USlimeEcologySubsystem* GetEcologySubsystem() const;

	void RefreshPerception();
	void UpdateBehavior();
	void ChooseExploreGoal();
	FVector ChooseAvoidGoal(const FVector& AwayDirection) const;
	FVector ChooseThreatAvoidGoal(const FVector& ThreatLocation) const;
	bool HasLineOfSight(const AActor* Target) const;
	bool HasLineOfSightToLocation(const FVector& Location) const;
	bool ProjectToNavigation(const FVector& Desired, FVector& OutLocation) const;
	bool UpdateResourceBehavior();
	bool UpdateCarryingBehavior();
	bool UpdatePaintBehavior();

	ESlimeEcologyBehavior Behavior = ESlimeEcologyBehavior::Paused;
	FVector HomeLocation = FVector::ZeroVector;
	FVector GroupCenter = FVector::ZeroVector;
	FVector DesiredGoal = FVector::ZeroVector;
	float ActivityRadius = 0.f;
	float DesiredSpeed = 0.f;
	float RefreshCountdown = 0.f;
	float ExploreGoalAge = 0.f;
	float AvoidGoalAge = 0.f;
	float ResourceMemoryRemaining = 0.f;
	float ResourceTargetHoldRemaining = 0.f;
	float GatherTimeRemaining = 0.f;
	float GatherRestRemaining = 0.f;
	float PlayerDistance = TNumericLimits<float>::Max();
	bool bPlayerVisible = false;
	bool bWasAlerted = false;
	bool bHasExploreGoal = false;
	TArray<TWeakObjectPtr<ASlimeEcologyEnemy>> VisibleAllies;
	TWeakObjectPtr<ASlimeMineralPatch> MineralTarget;
	TWeakObjectPtr<ASlimeCargoPickup> CargoTarget;
	TWeakObjectPtr<APawn> CachedPlayer;
	TWeakObjectPtr<ASlimeEcologyEnemy> NearestEnemy;
	TWeakObjectPtr<ASlimeEcologyEnemy> NearestHostileCore;
	FRandomStream RandomStream;
	TMap<FIntPoint, int32> ExplorationVisits;
	TMap<TWeakObjectPtr<ASlimeMineralPatch>, float> MineralTargetCooldownRemaining;
	int32 RefreshCount = 0;
};
