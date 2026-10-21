// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/CheatManager.h"
#include "SlimeCheatManager.generated.h"

class ASlimeEnemyBase;
class ASlimeEcologyEnemy;

/**
 * Phase 0 cheat commands used by the CP-0 acceptance pass.
 * Type these into the PIE console (backtick).
 */
UCLASS()
class USlimeCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** Print the loaded data tables, their paths and row names. */
	UFUNCTION(Exec)
	void SlimeDumpTables();

	/** Re-read the tables after editing DA_RunConfig / the DataTables. */
	UFUNCTION(Exec)
	void SlimeReloadTables();

	/** Apply Amount damage to the closest live enemy through the unified damage entry. */
	UFUNCTION(Exec)
	void SlimeDamageNearestEnemy(float Amount);

	/** Print capsule, mesh and ground heights for the M1 grounding acceptance check. */
	UFUNCTION(Exec)
	void SlimeDumpBodyMetrics();

	/** Spawn mass 1/4/8 in the open, then dump grounding metrics and take screenshots. */
	UFUNCTION(Exec)
	void SlimeGroundingTest();

	// -- M2: ecology locomotion --

	UFUNCTION(Exec)
	void SlimeSpawnEcology(int32 Count = 10, int32 Faction = 0, int32 Mass = 1);

	UFUNCTION(Exec)
	void SlimeEcologyStatus();

	UFUNCTION(Exec)
	void SlimeEcologyGroundProbe();

	UFUNCTION(Exec)
	void SlimeEcologyBodyStatus();

	UFUNCTION(Exec)
	void SlimeEcologyAimLock(int32 Locked = 1);

	UFUNCTION(Exec)
	void SlimeEcologyExternalPush(float Speed = 400.f);

	UFUNCTION(Exec)
	void SlimeEcologyStunTest(float Seconds = 1.5f);

	UFUNCTION(Exec)
	void SlimeEcologyBodyTest();

	UFUNCTION(Exec)
	void SlimeEcologySetCargo(int32 Amount = 1);

	UFUNCTION(Exec)
	void SlimeEcologyPriorityTest();

	UFUNCTION(Exec)
	void SlimeEcologyR5Test();

	UFUNCTION(Exec)
	void SlimeEcologyExternalConnectionTest();

	UFUNCTION(Exec)
	void SlimeEcologyShotCooldownTest();

	UFUNCTION(Exec)
	void SlimeEcologyCorePriorityTest();

	// -- R1: player controls --

	UFUNCTION(Exec)
	void SlimePlayerSprint(int32 Enabled = 1);

	UFUNCTION(Exec)
	void SlimePlayerAim(int32 Enabled = 1);

	UFUNCTION(Exec)
	void SlimePlayerJump();

	UFUNCTION(Exec)
	void SlimePlayerSwitchShoulder();

	UFUNCTION(Exec)
	void SlimePlayerControlStatus();

	UFUNCTION(Exec)
	void SlimePlayerJumpTest();

        UFUNCTION(Exec)
        void SlimeEcologyAlertTest();

        UFUNCTION(Exec)
        void SlimeEcologyDisputeTest();

        UFUNCTION(Exec)
        void SlimeEcologyHostileFusionTest();

        UFUNCTION(Exec)
        void SlimeWeaponDoesNotAlertTest();

        UFUNCTION(Exec)
        void SlimeGrenadeStatus();

        UFUNCTION(Exec)
        void SlimeThrowGrenade(int32 Type = 0);

        UFUNCTION(Exec)
	void SlimeGrenadeAssimilationTest(int32 Faction = 1);

        UFUNCTION(Exec)
        void SlimeGrenadeTornadoTest();

        UFUNCTION(Exec)
        void SlimeGrenadeTornadoWallTest();

        UFUNCTION(Exec)
        void SlimeEcologyScoreTest();

        UFUNCTION(Exec)
        void SlimePaintStatus();

        UFUNCTION(Exec)
        void SlimePaintTest();

	UFUNCTION(Exec)
	void SlimeWeaponStatus();

	UFUNCTION(Exec)
	void SlimeAimAssistStatus();

	UFUNCTION(Exec)
	void SlimeAimAssistScreenTest();

	UFUNCTION(Exec)
	void SlimeChargeTest(float Alpha = 0.f);

	UFUNCTION(Exec)
	void SlimeKnockbackTest();

	UFUNCTION(Exec)
	void SlimeImpactSplitTest(int32 Mass = 2);

	UFUNCTION(Exec)
	void SlimeImpactSplitCollisionTest(int32 Mass = 2);

	UFUNCTION(Exec)
	void SlimeSpawnEcologyCore(int32 Mass = 2, int32 CoreState = 1);

	UFUNCTION(Exec)
	void SlimeEcologySetCoreState(int32 CoreState = 1);

	UFUNCTION(Exec)
	void SlimeEcologyProvokePlayer(float Seconds = 6.f);

	UFUNCTION(Exec)
	void SlimeEcologyCoreStatus();

	UFUNCTION(Exec)
	void SlimeEcologyPounceTest();

        UFUNCTION(Exec)
        void SlimeEcologyPounceLockTest();

	UFUNCTION(Exec)
	void SlimeEcologyDevourTest();

	UFUNCTION(Exec)
	void SlimeEcologyDetonationTest();

	UFUNCTION(Exec)
	void SlimeEcologyWeakpointTest();

	UFUNCTION(Exec)
	void SlimeEcologyFusionStatus();

	UFUNCTION(Exec)
	void SlimeEcologyFusionReady();

	UFUNCTION(Exec)
	void SlimeEcologyForceFuse();

	UFUNCTION(Exec)
	void SlimeEcologyInterruptFusion();

	UFUNCTION(Exec)
	void SlimeEcologyInterruptTest();

	UFUNCTION(Exec)
	void SlimeEcologyPushPairSetup();

	UFUNCTION(Exec)
	void SlimeEcologyFusionTest();

	UFUNCTION(Exec)
	void SlimeEcologyAutoFusionTest();

	UFUNCTION(Exec)
	void SlimeEcologyForcedFusionTest();

	UFUNCTION(Exec)
	void SlimeEcologySoakTest(float Seconds = 60.f);

	UFUNCTION(Exec)
	void SlimeEcologyResourceStatus();

	/** Validate every regenerating mineral against every faction site using the runtime navmesh. */
	UFUNCTION(Exec)
	void SlimeEcologyNavigationValidation();

	UFUNCTION(Exec)
	void SlimeEcologyResourceSoakTest(float Seconds = 30.f);

	UFUNCTION(Exec)
	void SlimeEcologyDeliveryTest();

	UFUNCTION(Exec)
	void SlimeEcologyCargoDropTest();

	UFUNCTION(Exec)
	void SlimeEcologyResourceLedgerCheck();

	UFUNCTION(Exec)
	void SlimeEcologyAvoidanceTest();

	/** Destroy every live slime. */
	UFUNCTION(Exec)
	void SlimeClearEnemies();

	/** Kill the player through the normal damage path (for death-path testing). */
	UFUNCTION(Exec)
	void SlimeKillPlayer();

	/** Spawn Count ecology bodies of a given mass in front of the player (faction A). */
	UFUNCTION(Exec)
	void SlimeSpawnNormalAtMass(int32 Mass = 1, int32 Count = 1);

	/** Re-stat the nearest ecology body to Mass, keeping its health ratio. */
	UFUNCTION(Exec)
	void SlimeSetMass(int32 Mass);

	// -- Phase C: run flow (PC-06 / CP-3) --

	/** Start (or restart) the run without waiting for the auto-start. */
	UFUNCTION(Exec)
	void SlimeRunStart();

	/** End the run as a time-up. The player-death path is exercised by SlimeKillPlayer. */
	UFUNCTION(Exec)
	void SlimeRunEnd();

	/** Multiplier for the run timeline: 10 reaches the last batch in ~10 s. 1 = real time. */
	UFUNCTION(Exec)
	void SlimeRunTimeScale(float Scale);

	/** Print every number the CP-3 hand calculation needs (score per mass tier, point states). */
	UFUNCTION(Exec)
	void SlimeRunStatus();

	// -- Phase D: full-run loop (PD-01 ~ PD-13 / CP-4) --

	/** Skip the preparation screen and deploy to a drop point (0 = D1, 1 = D2). */
	UFUNCTION(Exec)
	void SlimeRunDeploy(int32 DropPointIndex);

	/** Print the settlement data (score / target / best / kills / cleared points / passed). */
	UFUNCTION(Exec)
	void SlimeRunResult();

	/** Walk the real PD-04 retry path: reload the level and reuse the current drop point. */
	UFUNCTION(Exec)
	void SlimeRetry();

	/** Jump the countdown (last-15-seconds hint, time-up ending). */
	UFUNCTION(Exec)
	void SlimeRunSetTime(float SecondsRemaining);

	/** Add score directly, to check that crossing the target does not end the run early. */
	UFUNCTION(Exec)
	void SlimeRunAddScore(int32 Points);

	/** Toggle the pause menu through the same path the P key uses. */
	UFUNCTION(Exec)
	void SlimePause();

private:
	void ValidateEcologyNavigationNow();

	/** Same-frame spawns are not always visible to the actor iterator yet; retry once next tick. */
	UFUNCTION()
	void RetryDumpBodyMetrics();

	UFUNCTION()
	void RetryDumpBodyMetricsLate();

	UFUNCTION()
	void FinishGroundingTest();

	UFUNCTION()
	void ApplyGroundingRuntimeMassChange();

	UFUNCTION()
	void FinishEcologySoak();

	UFUNCTION()
	void BeginEcologyAvoidanceContact();

	UFUNCTION()
	void ConfirmEcologyAvoidanceSubjects();

	UFUNCTION()
	void BeginEcologyAvoidanceRecovery();

	UFUNCTION()
	void FinishEcologyAvoidance();

	UFUNCTION()
	void AdvanceEcologyBodyTest();

	UFUNCTION()
	void AdvancePlayerJumpTest();

	UFUNCTION()
	void AdvanceEcologyPriorityTest();

	UFUNCTION()
	void AdvanceEcologyR5Test();

	UFUNCTION()
	void FinishEcologyExternalConnectionTest();

        UFUNCTION()
        void AdvanceEcologyGrenadeTest();

	UFUNCTION()
	void FinishEcologyResourceSoak();

	UFUNCTION()
	void FinishEcologyDeliveryTest();

	UFUNCTION()
	void FinishAimAssistScreenTest();

	UFUNCTION()
	void FinishImpactSplitCollisionTest();

	UFUNCTION()
	void AdvanceEcologyCoreTest();

        UFUNCTION()
        void AdvanceEcologyPounceLockTest();

	UFUNCTION()
	void AdvanceEcologyFusionTest();

	void DumpBodyMetricsNow();
	void RefreshEcologyTestList();
	ASlimeEcologyEnemy* FindNearestEcologyEnemy() const;
	bool SpawnEcologyPairForTest(
		int32 Mass,
		float Spacing,
		ASlimeEcologyEnemy*& OutFirst,
		ASlimeEcologyEnemy*& OutSecond);
	FVector ComputeEcologyGroupCentre() const;
	int32 CountStaticPenetrations() const;
	float ComputeMinimumPairDistance() const;
	float ComputeAverageGroupDistance() const;

	FTimerHandle BodyMetricsTimer;
	FTimerHandle BodyMetricsLateTimer;
	FTimerHandle GroundingTestTimer;
	FTimerHandle GroundingMassChangeTimer;
	FTimerHandle EcologySoakTimer;
	FTimerHandle EcologyAvoidanceTimer;
	FTimerHandle EcologyBodyTestTimer;
	FTimerHandle PlayerJumpTestTimer;
	FTimerHandle EcologyPriorityTestTimer;
	FTimerHandle EcologyR5TestTimer;
	FTimerHandle EcologyExternalConnectionTimer;
	FTimerHandle EcologyCorePriorityTimer;
        FTimerHandle EcologyGrenadeTestTimer;
        FTimerHandle EcologyAlertTestTimer;
	FTimerHandle EcologyResourceSoakTimer;
	FTimerHandle EcologyDeliveryTestTimer;
	FTimerHandle AimAssistTestTimer;
	FTimerHandle ImpactSplitCollisionTimer;
	FTimerHandle EcologyCoreTestTimer;
        FTimerHandle EcologyPounceLockTimer;
	FTimerHandle EcologyFusionTestTimer;
	TArray<TWeakObjectPtr<ASlimeEnemyBase>> GroundingTestSlimes;
	TArray<TWeakObjectPtr<ASlimeEcologyEnemy>> EcologyTestSlimes;
	TArray<TWeakObjectPtr<ASlimeEcologyEnemy>> EcologyAvoidanceSubjects;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyPrimaryAvoidanceSubject;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyBodyTestTarget;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyDeliveryTarget;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyFusionTestPrimary;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyFusionTestSecondary;
        TWeakObjectPtr<ASlimeEcologyEnemy> EcologyTornadoNear;
        TWeakObjectPtr<ASlimeEcologyEnemy> EcologyTornadoFar;
        TWeakObjectPtr<AActor> EcologyTornadoWall;
	FVector EcologyAimLockAnchor = FVector::ZeroVector;
	TWeakObjectPtr<AActor> ImpactSplitCollisionWall;
	int32 ImpactSplitCollisionParentMass = 0;
	FVector EcologyTestCentre = FVector::ZeroVector;
	float EcologySoakStartTime = 0.f;
	int32 EcologyPathRequestsAtStart = 0;
	int32 EcologyBodyTestStage = 0;
	int32 PlayerJumpTestStage = 0;
	int32 EcologyPriorityTestStage = 0;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyPriorityCarrier;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyR5Subject;
	int32 EcologyR5TestStage = 0;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyExternalFirst;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyExternalSecond;
        int32 EcologyGrenadeTestStage = 0;
	int32 EcologyDeliveryBaseline = 0;
	int32 EcologyCoreTestStage = 0;
        int32 EcologyPounceLockStage = 0;
        float EcologyPounceLockStartZ = 0.f;
	TWeakObjectPtr<ASlimeEcologyEnemy> EcologyCoreTestCore;
	int32 EcologyFusionTestStage = 0;
	ESlimeFaction EcologyGrenadeTestFaction = ESlimeFaction::B;
	bool bEcologyFusionForcedTest = false;
	bool bEcologyFusionAutoTest = false;
	bool bEcologyFusionInterruptTest = false;
	int32 GroundingTestStage = 0;

public:
	// -- Audio --

	/** Print the resolved audio events and which config object is in use. */
	UFUNCTION(Exec)
	void SlimeAudioDump();

	/**
	 * Play one audio event on the player pawn.
	 * Index as printed by SlimeAudioDump; Loop=1 toggles a looping event on and off.
	 */
	UFUNCTION(Exec)
	void SlimeAudioTest(int32 Index = 0, int32 Loop = 0);

private:
	uint32 AudioTestLoopID = 0;
};
