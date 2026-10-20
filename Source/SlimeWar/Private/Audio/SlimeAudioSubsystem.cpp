// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/SlimeAudioSubsystem.h"

#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "Core/SlimeWarLog.h"
#include "Engine/World.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** "Never played" sentinel. Zero would collide with the first frames of a world. */
	constexpr double NeverPlayed = -1.0e9;
}

USlimeAudioSubsystem* USlimeAudioSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	return World ? World->GetSubsystem<USlimeAudioSubsystem>() : nullptr;
}

void USlimeAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	for (double& Timestamp : LastPlaySeconds)
	{
		Timestamp = NeverPlayed;
	}

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (Settings && !Settings->AudioConfig.IsNull())
	{
		Config = Settings->AudioConfig.LoadSynchronous();
		if (!Config)
		{
			UE_LOG(LogSlimeWar, Warning,
				TEXT("SlimeAudio: AudioConfig '%s' failed to load; using class defaults."),
				*Settings->AudioConfig.ToString());
		}
	}

	if (!Config)
	{
		// The defaults already point at the real event paths, so an unconfigured project still
		// makes sound - it just cannot be repointed from the editor. GetDefault<> returns a const
		// pointer; the CDO is never mutated, so stripping the const here is safe.
		Config = const_cast<USlimeAudioConfig*>(GetDefault<USlimeAudioConfig>());
		UE_LOG(LogSlimeWar, Log,
			TEXT("SlimeAudio: no DA_AudioConfig configured, using USlimeAudioConfig defaults."));
	}
}

void USlimeAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!Config || !Config->bPrewarmEvents)
	{
		return;
	}

	const int32 Total = static_cast<int32>(ESlimeAudioEvent::Count);
	int32 Resolved = 0;
	for (int32 Index = 0; Index < Total; ++Index)
	{
		if (UAkAudioEvent* Event = Config->GetEvent(static_cast<ESlimeAudioEvent>(Index)))
		{
			// Resolve the media now rather than on the first shot: a sync load mid-fight is a hitch.
			Event->LoadData();
			++Resolved;
		}
	}

	UE_LOG(LogSlimeWar, Log, TEXT("SlimeAudio: config=%s, %d/%d events resolved."),
		*GetNameSafe(Config), Resolved, Total);
}

uint32 USlimeAudioSubsystem::PlayEvent(ESlimeAudioEvent InEvent, const AActor* Emitter)
{
	return PostInternal(Resolve(InEvent), Emitter);
}

uint32 USlimeAudioSubsystem::PlayThrottledEvent(ESlimeAudioEvent InEvent, const AActor* Emitter)
{
	if (!ConsumeThrottle(InEvent))
	{
		return InvalidPlayingID;
	}

	return PostInternal(Resolve(InEvent), Emitter);
}

uint32 USlimeAudioSubsystem::PlayGatedEvent(ESlimeAudioEvent InEvent, const AActor* Emitter)
{
	// Distance is checked first on purpose: the throttle must only record events that actually
	// played. Recording a distant one would silence the near one that follows in the same window.
	if (!IsAudible(Emitter) || !ConsumeThrottle(InEvent))
	{
		return InvalidPlayingID;
	}

	return PostInternal(Resolve(InEvent), Emitter);
}

uint32 USlimeAudioSubsystem::PlayLoop(ESlimeAudioEvent InEvent, const AActor* Emitter)
{
	// No throttle: the caller keeps its own dwell time, so a loop starts at most once per window.
	if (!IsAudible(Emitter))
	{
		return InvalidPlayingID;
	}

	return PostInternal(Resolve(InEvent), Emitter);
}

void USlimeAudioSubsystem::StopLoop(uint32& InOutPlayingID, float FadeOutSeconds)
{
	if (InOutPlayingID == InvalidPlayingID)
	{
		return;
	}

	if (FAkAudioDevice* Device = FAkAudioDevice::Get())
	{
		// Stop this one playing id only. UAkComponent::Stop would also cut the one-shot fusion
		// sound that is playing on the same slime at the same time.
		Device->StopPlayingID(
			static_cast<AkPlayingID>(InOutPlayingID),
			static_cast<AkTimeMs>(FMath::Max(0.f, FadeOutSeconds) * 1000.f),
			AkCurveInterpolation_Linear);
	}

	InOutPlayingID = InvalidPlayingID;
}

bool USlimeAudioSubsystem::IsAudible(const AActor* Emitter) const
{
	if (!IsValid(Emitter))
	{
		return false;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		// Menus and cinematics have no pawn; cut nothing rather than going silent everywhere.
		return true;
	}

	const float Radius = Config ? Config->AudibleRadius : 2500.f;
	return FVector::DistSquared(Emitter->GetActorLocation(), Player->GetActorLocation())
		<= Radius * Radius;
}

bool USlimeAudioSubsystem::ConsumeThrottle(ESlimeAudioEvent InEvent)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const float Window = Config ? Config->ThrottleSeconds : 0.15f;

	double& Last = LastPlaySeconds[static_cast<uint8>(InEvent)];
	if (Now - Last < Window)
	{
		return false;
	}

	Last = Now;
	return true;
}

UAkAudioEvent* USlimeAudioSubsystem::Resolve(ESlimeAudioEvent InEvent) const
{
	return Config ? Config->GetEvent(InEvent) : nullptr;
}

uint32 USlimeAudioSubsystem::PostInternal(UAkAudioEvent* Event, const AActor* Emitter) const
{
	// A null actor is not an error to Wwise: it plays the event as an ambient sound, which ignores
	// attenuation and is heard at full volume from anywhere. Refuse instead.
	if (!Event || !IsValid(Emitter))
	{
		return InvalidPlayingID;
	}

	const int32 PlayingID = UAkGameplayStatics::PostEvent(
		Event,
		const_cast<AActor*>(Emitter),
		/*CallbackMask=*/0,
		FOnAkPostEventCallback(),
		/*bStopWhenAttachedToDestroyed=*/true);

	if (PlayingID <= 0)
	{
		return InvalidPlayingID;
	}

	UE_LOG(LogSlimeWar, Verbose, TEXT("SlimeAudio: post %s on %s -> id %d"),
		*Event->GetName(), *GetNameSafe(Emitter), PlayingID);

	return static_cast<uint32>(PlayingID);
}
