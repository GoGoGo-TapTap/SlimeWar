// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Audio/SlimeAudioConfig.h"
#include "SlimeAudioSubsystem.generated.h"

class AActor;
class UAkAudioEvent;

/**
 * Single place that turns "a gameplay thing happened" into a Wwise post.
 *
 * A world subsystem rather than a game instance one: the throttle bookkeeping belongs to one run,
 * and destroying the world must clear it. Every level must start with a clean slate, otherwise the
 * first fusion of the next attempt would be swallowed by the previous run's timestamp.
 *
 * Note the three entry points. They are separate functions rather than one with a bool because the
 * distinction is semantic, not a flag: the player's own sounds always play, a dry-fire click only
 * needs de-duplication, and the ecology has to be filtered by distance as well.
 */
UCLASS()
class SLIMEWAR_API USlimeAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Wwise uses 0 as "no such playing id". */
	static constexpr uint32 InvalidPlayingID = 0;

	/** Instance for the world the context object lives in; null outside a game world. */
	static USlimeAudioSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	const USlimeAudioConfig* GetConfig() const { return Config; }

	/** Play without any gate. Weapon fire and reload: every occurrence must be heard. */
	uint32 PlayEvent(ESlimeAudioEvent InEvent, const AActor* Emitter);

	/** Play unless the same event played within ThrottleSeconds (dry-fire spam). */
	uint32 PlayThrottledEvent(ESlimeAudioEvent InEvent, const AActor* Emitter);

	/** Play only when close enough to the player AND not throttled. Ecology sounds. */
	uint32 PlayGatedEvent(ESlimeAudioEvent InEvent, const AActor* Emitter);

	/** Start a loop unless it is out of earshot. Returns the id to hand back to StopLoop. */
	uint32 PlayLoop(ESlimeAudioEvent InEvent, const AActor* Emitter);

	/** Stop one specific loop and clear the handle. Safe to call with an invalid id. */
	void StopLoop(uint32& InOutPlayingID, float FadeOutSeconds = 0.05f);

	/** Is this emitter close enough to the player to be worth a voice? */
	bool IsAudible(const AActor* Emitter) const;

	/** True when this event may play now; has the side effect of recording the time. */
	bool ConsumeThrottle(ESlimeAudioEvent InEvent);

private:
	UAkAudioEvent* Resolve(ESlimeAudioEvent InEvent) const;
	uint32 PostInternal(UAkAudioEvent* Event, const AActor* Emitter) const;

	/** Resolved once in Initialize; never null, falls back to the class defaults. */
	UPROPERTY(Transient)
	TObjectPtr<USlimeAudioConfig> Config;

	double LastPlaySeconds[static_cast<uint8>(ESlimeAudioEvent::Count)] = {};
};
