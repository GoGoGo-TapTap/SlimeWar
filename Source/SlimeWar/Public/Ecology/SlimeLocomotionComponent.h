// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeLocomotionComponent.generated.h"

class ASlimeEcologyEnemy;
class USlimeBodyInteractionComponent;
class USlimeEcologyBrainComponent;
class USlimeEcologyRules;

/** Cached waypoint navigation plus frame-local steering for one ecology slime. */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeLocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeLocomotionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void SetMoveGoal(const FVector& InGoal, float InSpeed, float InArrivalRadius = -1.f);
	void ClearMoveGoal();
	void SuspendVoluntaryMovement();

	bool HasMoveGoal() const { return bHasGoal; }
	bool IsAtGoal() const { return bAtGoal; }
	bool ConsumeStuckExhausted();

	const FVector& GetGoal() const { return Goal; }
	const FVector& GetCurrentWaypoint() const { return CurrentWaypoint; }
	const FVector& GetDesiredDirection() const { return SmoothedDirection; }
	float GetDesiredSpeed() const { return DesiredSpeed; }
	int32 GetPathPointCount() const { return PathPoints.Num(); }
	int32 GetPathIndex() const { return PathIndex; }
	int32 GetPathFailureCount() const { return PathFailures; }
	int32 GetPathRequestCount() const { return PathRequestCount; }
	float GetMaxStuckTime() const { return MaxStuckTime; }
	float GetCurrentStuckTime() const { return TimeWithoutProgress; }

private:
	ASlimeEcologyEnemy* GetOwnerEnemy() const;
	USlimeEcologyBrainComponent* GetBrain() const;
	USlimeBodyInteractionComponent* GetBodyInteraction() const;
	const USlimeEcologyRules* GetRules() const;

	void RefreshPath();
	void HandleStuck();
	bool IsDirectPathClear(const FVector& From, const FVector& To) const;
	bool BuildNavPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const;
	void StopAtGoal();

	bool bHasGoal = false;
	bool bAtGoal = false;
	bool bStuckExhausted = false;
	bool bStopped = true;
	bool bAutonomousSuspended = false;
	FVector Goal = FVector::ZeroVector;
	FVector CurrentWaypoint = FVector::ZeroVector;
	FVector SmoothedDirection = FVector::ZeroVector;
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	int32 PathFailures = 0;
	int32 PathRequestCount = 0;
	float DesiredSpeed = 0.f;
	float GoalArrivalRadius = -1.f;
	float PathRefreshRemaining = 0.f;
	FVector ProgressAnchor = FVector::ZeroVector;
	float TimeWithoutProgress = 0.f;
	float MaxStuckTime = 0.f;
};
