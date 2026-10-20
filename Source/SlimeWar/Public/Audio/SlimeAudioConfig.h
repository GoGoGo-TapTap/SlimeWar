// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeAudioConfig.generated.h"

class UAkAudioEvent;

/**
 * The gameplay sounds the C++ side can post.
 *
 * One enum value per trigger point rather than one field per event: call sites then read as
 * "play MonsterMerge", and the asset table stays a single flat list you can review at a glance.
 *
 * The last five are already wired in Blueprint (member / HUD); they are listed here so the
 * data asset is complete and picking them up later needs no C++ change.
 */
UENUM(BlueprintType)
enum class ESlimeAudioEvent : uint8
{
	WeaponFire,
	WeaponEmpty,
	WeaponReload,

	ScoutReport,
	MonsterAttack,
	MonsterMerge,
	MonsterMergeEnd,
	ResourceCollect,
	ResourceDeliver,
	MonsterMove,

	PlayerHit,
	PlayerDeath,
	PlayerHealth,
	PlayerFootstep,
	WeaponHit,

	Count UMETA(Hidden)
};

/**
 * Event table plus the gate tuning that decides when a sound is allowed to play.
 *
 * Every field defaults to the real Wwise asset path, so GetDefault<>() is a working configuration:
 * the project makes sound even before anyone creates a DA_AudioConfig asset. Creating the asset
 * only exists to let sound design repoint events without a recompile.
 */
UCLASS(BlueprintType)
class SLIMEWAR_API USlimeAudioConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USlimeAudioConfig();

	/**
	 * Resolve one event. Null when the asset is missing.
	 *
	 * A missing event is logged once per event, not once per call: an ecology sound fires dozens of
	 * times a second and would otherwise bury the log.
	 */
	UAkAudioEvent* GetEvent(ESlimeAudioEvent InEvent) const;

	// -- Events --

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Weapon")
	TSoftObjectPtr<UAkAudioEvent> WeaponFire;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Weapon")
	TSoftObjectPtr<UAkAudioEvent> WeaponEmpty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Weapon")
	TSoftObjectPtr<UAkAudioEvent> WeaponReload;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> ScoutReport;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> MonsterAttack;

	/** Fusion starts: two slimes begin converging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> MonsterMerge;

	/** Fusion committed: the two bodies actually became one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> MonsterMergeEnd;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> ResourceCollect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> ResourceDeliver;

	/** Looping movement bed. Must be a looping event in Wwise, otherwise it can only be cut short. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Ecology")
	TSoftObjectPtr<UAkAudioEvent> MonsterMove;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Player")
	TSoftObjectPtr<UAkAudioEvent> PlayerHit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Player")
	TSoftObjectPtr<UAkAudioEvent> PlayerDeath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Player")
	TSoftObjectPtr<UAkAudioEvent> PlayerHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Player")
	TSoftObjectPtr<UAkAudioEvent> PlayerFootstep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events|Player")
	TSoftObjectPtr<UAkAudioEvent> WeaponHit;

	// -- Gates --

	/**
	 * Minimum gap between two posts of the same event.
	 *
	 * Three slimes fusing in the same frame would otherwise stack three copies of the same clip on
	 * top of each other, which reads as a single loud pop rather than three events.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	float ThrottleSeconds = 0.15f;

	/**
	 * Only post within this distance of the player.
	 *
	 * The ecology runs ~30 individuals; most of them are too far away to be heard, and every post
	 * still costs a Wwise voice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	float AudibleRadius = 2500.f;

	// -- Movement loop --

	/** Speed (cm/s) above which the movement loop starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loop")
	float MoveLoopEnterSpeed = 40.f;

	/** Speed below which it stops. Lower than the enter value on purpose: hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loop")
	float MoveLoopExitSpeed = 15.f;

	/** How long the enter/exit condition must hold, so a single slow frame cannot cut the loop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loop")
	float MoveLoopDwellSeconds = 0.2f;

	/** Resolve every event once at world start so the first post does not hitch on a load. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loop")
	bool bPrewarmEvents = true;

private:
	/** Events already reported missing; mutable because GetEvent is const. */
	mutable TSet<uint8> WarnedMissing;
};
