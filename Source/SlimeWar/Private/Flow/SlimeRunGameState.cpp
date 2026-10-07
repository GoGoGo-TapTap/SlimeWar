// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/SlimeRunGameState.h"

#include "Core/SlimeWarLog.h"
#include "Flow/RunSubsystem.h"
#include "Flow/ScoreSubsystem.h"

void ASlimeRunGameState::BeginPlay()
{
	Super::BeginPlay();

	// Pull before binding: the run may already have started during world begin play (the subsystem
	// is created before any actor), and this also covers a HUD that binds much later.
	PullSnapshot();
	BindSubsystems();
}

void ASlimeRunGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSubsystems();

	Super::EndPlay(EndPlayReason);
}

void ASlimeRunGameState::PullSnapshot()
{
	if (const URunSubsystem* Run = URunSubsystem::Get(this))
	{
		RunState = Run->GetRunState();
		RemainingSeconds = Run->GetRemainingSeconds();
		Run->GetPointSnapshot(PointIds, PointStates);
	}

	if (const UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		CurrentScore = Score->GetCurrentScore();
		BestScore = Score->GetBestScore();
		TargetScore = Score->GetTargetScore();
		NormalKillCount = Score->GetNormalKillCount();
		ClearedPointCount = Score->GetClearedPointCount();
	}
}

void ASlimeRunGameState::BindSubsystems()
{
	UnbindSubsystems();

	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->OnRunStateChanged.AddUObject(this, &ASlimeRunGameState::HandleRunStateChanged);
		Run->OnRunEnded.AddUObject(this, &ASlimeRunGameState::HandleRunEnded);
		Run->OnTimeChanged.AddUObject(this, &ASlimeRunGameState::HandleTimeChanged);
		Run->OnPointStateChanged.AddUObject(this, &ASlimeRunGameState::HandlePointStateChanged);
		Run->OnBatchIncoming.AddUObject(this, &ASlimeRunGameState::HandleBatchIncoming);
		BoundRunSubsystem = Run;
	}

	if (UScoreSubsystem* Score = UScoreSubsystem::Get(this))
	{
		Score->OnScoreChanged.AddUObject(this, &ASlimeRunGameState::HandleScoreChanged);
		Score->OnBestScoreChanged.AddUObject(this, &ASlimeRunGameState::HandleBestScoreChanged);
		Score->OnNormalKillCountChanged.AddUObject(this, &ASlimeRunGameState::HandleNormalKillCountChanged);
		Score->OnClearedPointCountChanged.AddUObject(this, &ASlimeRunGameState::HandleClearedPointCountChanged);
		BoundScoreSubsystem = Score;
	}
}

void ASlimeRunGameState::UnbindSubsystems()
{
	if (URunSubsystem* Run = BoundRunSubsystem.Get())
	{
		Run->OnRunStateChanged.RemoveAll(this);
		Run->OnRunEnded.RemoveAll(this);
		Run->OnTimeChanged.RemoveAll(this);
		Run->OnPointStateChanged.RemoveAll(this);
		Run->OnBatchIncoming.RemoveAll(this);
	}
	BoundRunSubsystem.Reset();

	if (UScoreSubsystem* Score = BoundScoreSubsystem.Get())
	{
		Score->OnScoreChanged.RemoveAll(this);
		Score->OnBestScoreChanged.RemoveAll(this);
		Score->OnNormalKillCountChanged.RemoveAll(this);
		Score->OnClearedPointCountChanged.RemoveAll(this);
	}
	BoundScoreSubsystem.Reset();
}

void ASlimeRunGameState::HandleScoreChanged(int32 NewScore)
{
	CurrentScore = NewScore;
	OnScoreChanged.Broadcast(CurrentScore);
}

void ASlimeRunGameState::HandleBestScoreChanged(int32 NewBestScore)
{
	BestScore = NewBestScore;
	OnBestScoreChanged.Broadcast(BestScore);
}

void ASlimeRunGameState::HandleNormalKillCountChanged(int32 NewCount)
{
	NormalKillCount = NewCount;
	OnNormalKillCountChanged.Broadcast(NormalKillCount);
}

void ASlimeRunGameState::HandleClearedPointCountChanged(int32 NewCount)
{
	ClearedPointCount = NewCount;
	OnClearedPointCountChanged.Broadcast(ClearedPointCount);
}

void ASlimeRunGameState::HandleTimeChanged(int32 NewRemainingSeconds)
{
	RemainingSeconds = NewRemainingSeconds;
	OnTimeChanged.Broadcast(RemainingSeconds);
}

void ASlimeRunGameState::HandlePointStateChanged(int32 PointId, ESpawnPointState NewState)
{
	SetPointState(PointId, NewState);
	OnPointStateChanged.Broadcast(PointId, NewState);
}

void ASlimeRunGameState::HandleRunStateChanged(ESlimeRunState NewState)
{
	RunState = NewState;
	OnRunStateChanged.Broadcast(RunState);
}

void ASlimeRunGameState::HandleRunEnded(ERunEndReason Reason)
{
	EndReason = Reason;

	// The score and the point states may have changed in the same frame as the end.
	PullSnapshot();
	OnRunEnded.Broadcast(Reason);
}

void ASlimeRunGameState::HandleBatchIncoming(int32 BatchIndex, float SecondsUntilSpawn)
{
	OnBatchIncoming.Broadcast(BatchIndex, SecondsUntilSpawn);
}

void ASlimeRunGameState::SetPointState(int32 PointId, ESpawnPointState NewState)
{
	const int32 Index = PointIds.IndexOfByKey(PointId);
	if (Index == INDEX_NONE)
	{
		PointIds.Add(PointId);
		PointStates.Add(NewState);
		return;
	}

	PointStates[Index] = NewState;
}

ESpawnPointState ASlimeRunGameState::GetPointState(int32 PointId) const
{
	const int32 Index = PointIds.IndexOfByKey(PointId);
	return PointStates.IsValidIndex(Index) ? PointStates[Index] : ESpawnPointState::AwaitingDeploy;
}
