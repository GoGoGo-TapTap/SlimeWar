// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/ScoreSubsystem.h"

#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/SlimeFlowMath.h"
#include "GameplayFramework/StatTableProvider.h"

UScoreSubsystem* UScoreSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UScoreSubsystem>() : nullptr;
}

void UScoreSubsystem::BeginRun(int32 InTargetScore)
{
	CurrentScore = 0;
	NormalKillCount = 0;
	PlayerKillCount = 0;
	NpcKillCount = 0;
	PlayerCargoScored = 0;
	HostileKillCount = 0;
	DetonationKillCount = 0;
	ChainKillCount = 0;
	ClearedPointCount = 0;
	TargetScore = InTargetScore;
	bScoreLocked = false;

	KillsByMass.Init(0, MaxTrackedMass + 1);
	ScoreByMass.Init(0, MaxTrackedMass + 1);

	OnScoreChanged.Broadcast(CurrentScore);
	OnNormalKillCountChanged.Broadcast(NormalKillCount);
	OnClearedPointCountChanged.Broadcast(ClearedPointCount);
}

void UScoreSubsystem::AddKill(const FSlimeKillContext& Context)
{
	if (bScoreLocked)
	{
		return;
	}

	if (!Context.bKilledByPlayer)
	{
		++NpcKillCount;
		return;
	}

	const int32 Mass = FMath::Max(1, Context.Mass);
	const int32 Awarded = SlimeFlowMath::EcologyKillScore(Context);

	CurrentScore += Awarded;
	++NormalKillCount;
	++PlayerKillCount;
	PlayerCargoScored += FMath::Max(0, Context.Cargo);
	HostileKillCount += Context.bHostileCore ? 1 : 0;
	DetonationKillCount += Context.bDetonated ? 1 : 0;
	ChainKillCount += Context.bChainKill ? 1 : 0;

	if (KillsByMass.IsValidIndex(Mass))
	{
		++KillsByMass[Mass];
		ScoreByMass[Mass] += Awarded;
	}

	OnScoreChanged.Broadcast(CurrentScore);
	OnNormalKillCountChanged.Broadcast(NormalKillCount);
}

void UScoreSubsystem::GetKillBreakdown(TArray<int32>& OutKillsByMass, TArray<int32>& OutScoreByMass) const
{
	OutKillsByMass = KillsByMass;
	OutScoreByMass = ScoreByMass;
}

void UScoreSubsystem::AddClearedPoint()
{
	if (bScoreLocked)
	{
		return;
	}

	++ClearedPointCount;
	OnClearedPointCountChanged.Broadcast(ClearedPointCount);
}

void UScoreSubsystem::DebugAddScore(int32 Points)
{
	if (bScoreLocked)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("UScoreSubsystem::DebugAddScore: scoring is locked, ignored."));
		return;
	}

	CurrentScore += FMath::Max(0, Points);
	OnScoreChanged.Broadcast(CurrentScore);

	UE_LOG(LogSlimeWar, Log, TEXT("UScoreSubsystem: debug score jump, now %d / %d."),
		CurrentScore, TargetScore);
}

void UScoreSubsystem::ResolveBestScore(ERunEndReason Reason)
{
	if (!SlimeFlowMath::ShouldRefreshBestScore(Reason, CurrentScore, TargetScore))
	{
		return;
	}

	if (CurrentScore <= BestScore)
	{
		return;
	}

	BestScore = CurrentScore;
	OnBestScoreChanged.Broadcast(BestScore);
}

void UScoreSubsystem::Deinitialize()
{
	OnScoreChanged.Clear();
	OnNormalKillCountChanged.Clear();
	OnClearedPointCountChanged.Clear();
	OnBestScoreChanged.Clear();

	Super::Deinitialize();
}
