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
	ClearedPointCount = 0;
	TargetScore = InTargetScore;
	bScoreLocked = false;

	KillsByMass.Init(0, MaxTrackedMass + 1);
	ScoreByMass.Init(0, MaxTrackedMass + 1);

	OnScoreChanged.Broadcast(CurrentScore);
	OnNormalKillCountChanged.Broadcast(NormalKillCount);
	OnClearedPointCountChanged.Broadcast(ClearedPointCount);
}

void UScoreSubsystem::AddKill(ETargetKind Kind, int32 Mass)
{
	if (bScoreLocked || Kind != ETargetKind::Normal)
	{
		return;
	}

	// The enemy only reports "a normal target of this mass died"; the worth of that kill is a
	// table value, exactly like every other number in this project.
	const UStatTableProvider* Provider =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;

	FSlimeStatRow Row;
	if (!Provider || !Provider->GetSlimeStat(Mass, Row))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("UScoreSubsystem::AddKill: no stat row for mass %d, the kill is ignored."), Mass);
		return;
	}

	const int32 Awarded = SlimeFlowMath::ScoreForKill(Kind, Row.KillScore);
	CurrentScore += Awarded;
	++NormalKillCount;

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
