// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimePresentationDirector.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flow/RunSubsystem.h"
#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "TimerManager.h"

namespace
{
	/**
	 * Camera blend back to the player once the deployment landed (design 2.3: 0.5 s).
	 * Presentation only - the player already has control when this starts.
	 */
	constexpr float LandingBlendTime = 0.5f;

	/** How far the sequence length may drift from the data value before we complain. */
	constexpr double LengthToleranceSeconds = 0.05;
}

USlimePresentationDirector* USlimePresentationDirector::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	return World ? World->GetSubsystem<USlimePresentationDirector>() : nullptr;
}

void USlimePresentationDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The GameState is spawned by the game mode during StartPlay, so the binding waits a tick.
	InWorld.GetTimerManager().SetTimerForNextTick(this, &USlimePresentationDirector::Setup);
}

void USlimePresentationDirector::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeployTimerHandle);
	}
	DeployTimerHandle.Invalidate();

	if (ASlimeRunGameState* RunState = GetWorld() ? GetWorld()->GetGameState<ASlimeRunGameState>() : nullptr)
	{
		RunState->OnRunStateChanged.RemoveAll(this);
	}

	StopCurrentSequence();
	bDeploymentPending = false;

	Super::Deinitialize();
}

void USlimePresentationDirector::Setup()
{
	if (ASlimeRunGameState* RunState = GetWorld() ? GetWorld()->GetGameState<ASlimeRunGameState>() : nullptr)
	{
		RunState->OnRunStateChanged.AddUObject(this, &USlimePresentationDirector::HandleRunStateChanged);

		// A retry reloads the level and jumps straight into Deploying before this subsystem ever
		// runs, so the current phase has to be applied once here as well.
		HandleRunStateChanged(RunState->GetRunState());
	}
	else
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePresentationDirector: no ASlimeRunGameState; the cinematics will not play."));
	}
}

void USlimePresentationDirector::HandleRunStateChanged(ESlimeRunState NewState)
{
	switch (NewState)
	{
	case ESlimeRunState::Deploying:
		PlayDeployment();
		break;

	case ESlimeRunState::Result:
		PlayResult();
		break;

	default:
		break;
	}
}

void USlimePresentationDirector::PlayDeployment()
{
	StopCurrentSequence();

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	const USlimeRunConfig* RunConfig = Settings ? Settings->RunConfig.LoadSynchronous() : nullptr;
	const float Duration = RunConfig ? FMath::Max(0.f, RunConfig->DeployDuration) : 0.f;

	const URunSubsystem* Run = URunSubsystem::Get(this);
	const int32 DropIndex = Run ? Run->GetSelectedDropPoint() : INDEX_NONE;

	ULevelSequence* Sequence = nullptr;
	if (Settings && Settings->DeploySequences.IsValidIndex(DropIndex))
	{
		Sequence = Cast<ULevelSequence>(Settings->DeploySequences[DropIndex].TryLoad());
	}

	if (Sequence)
	{
		WarnIfLengthDiffers(Sequence, Duration, TEXT("DeployDuration"));
		StartSequence(Sequence);
	}
	else
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePresentationDirector: no deploy sequence for drop point %d; skipping the cinematic."),
			DropIndex);
	}

	bDeploymentPending = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishDeployment();
		return;
	}

	if (Duration > 0.f)
	{
		// The data value is the gate: the sequence may be shorter or longer, the run starts here.
		World->GetTimerManager().SetTimer(
			DeployTimerHandle, this, &USlimePresentationDirector::FinishDeployment, Duration, /*bLoop=*/false);
	}
	else
	{
		// Unconfigured data (or an asset-less code pass): do not hold the player hostage.
		FinishDeployment();
	}
}

void USlimePresentationDirector::PlayResult()
{
	StopCurrentSequence();

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	ULevelSequence* Sequence = Settings ? Cast<ULevelSequence>(Settings->ResultSequence.TryLoad()) : nullptr;

	if (!Sequence)
	{
		// URunSubsystem still owns the Result -> Ended timing, so nothing else is needed here.
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePresentationDirector: no result sequence; the settlement screen will appear immediately."));
		return;
	}

	const USlimeRunConfig* RunConfig = Settings ? Settings->RunConfig.LoadSynchronous() : nullptr;
	WarnIfLengthDiffers(Sequence, RunConfig ? RunConfig->ResultOrbitDuration : 0.f, TEXT("ResultOrbitDuration"));

	StartSequence(Sequence);
}

bool USlimePresentationDirector::StartSequence(ULevelSequence* Sequence)
{
	UWorld* World = GetWorld();
	if (!World || !Sequence)
	{
		return false;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	PlaybackSettings.bAutoPlay = false;
	PlaybackSettings.bDisableCameraCuts = false;

	ALevelSequenceActor* NewActor = nullptr;
	ULevelSequencePlayer* NewPlayer =
		ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, PlaybackSettings, NewActor);

	if (!NewPlayer)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePresentationDirector: failed to create a player for '%s'."), *GetNameSafe(Sequence));
		return false;
	}

	SequencePlayer = NewPlayer;
	SequenceActor = NewActor;
	bPlaying = true;

	NewPlayer->Play();
	return true;
}

void USlimePresentationDirector::StopCurrentSequence()
{
	if (ULevelSequencePlayer* Player = SequencePlayer)
	{
		Player->Stop();
	}
	SequencePlayer = nullptr;

	if (ALevelSequenceActor* Actor = SequenceActor)
	{
		Actor->Destroy();
	}
	SequenceActor = nullptr;

	bPlaying = false;
}

void USlimePresentationDirector::FinishDeployment()
{
	if (!bDeploymentPending)
	{
		return;
	}

	bDeploymentPending = false;
	StopCurrentSequence();

	// Control first (design 5.2: the first batch appears with player control), then the 0.5 s
	// camera blend - which is why the blend never blocks input.
	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->ConfirmDeployment();
	}

	BlendBackToPlayer();
}

void USlimePresentationDirector::BlendBackToPlayer()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (PC && Pawn)
	{
		PC->SetViewTargetWithBlend(Pawn, LandingBlendTime, EViewTargetBlendFunction::VTBlend_Cubic);
	}
}

void USlimePresentationDirector::WarnIfLengthDiffers(
	const ULevelSequence* Sequence, float ExpectedSeconds, const TCHAR* DataName) const
{
	if (!Sequence || ExpectedSeconds <= 0.f)
	{
		return;
	}

	const UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const double TicksPerSecond = TickResolution.AsDecimal();
	if (TicksPerSecond <= 0.0)
	{
		return;
	}

	const TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	const double LengthSeconds =
		static_cast<double>((Range.GetUpperBoundValue() - Range.GetLowerBoundValue()).Value) / TicksPerSecond;

	if (!FMath::IsNearlyEqual(LengthSeconds, static_cast<double>(ExpectedSeconds), LengthToleranceSeconds))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("USlimePresentationDirector: '%s' is %.2fs but %s is %.2fs; the data value is what gates ")
			TEXT("gameplay, the sequence is presentation only."),
			*GetNameSafe(Sequence), LengthSeconds, DataName, ExpectedSeconds);
	}
}
