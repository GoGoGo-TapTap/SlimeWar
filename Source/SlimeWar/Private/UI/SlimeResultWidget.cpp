// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeResultWidget.h"

#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "UI/SlimeUISubsystem.h"

void USlimeResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshResult();
}

void USlimeResultWidget::RefreshResult()
{
	// The run is over, so the GameState already holds the final numbers (EndRun pushes them before
	// the orbit plays, and the phase only reaches Ended afterwards).
	FSlimeRunResult Result;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ASlimeRunGameState* RunState = PC->GetWorld()->GetGameState<ASlimeRunGameState>())
		{
			Result = RunState->GetRunResult();
		}
	}

	OnResultReady(Result);
}

bool USlimeResultWidget::WasPassed() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ASlimeRunGameState* RunState = PC ? PC->GetWorld()->GetGameState<ASlimeRunGameState>() : nullptr;
	return RunState ? RunState->GetRunResult().bPassed : false;
}

void USlimeResultWidget::RequestRetry()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestRetry();
	}
}

void USlimeResultWidget::RequestReselectDropPoint()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestReselectDropPoint();
	}
}

void USlimeResultWidget::RequestQuit()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestQuit();
	}
}
