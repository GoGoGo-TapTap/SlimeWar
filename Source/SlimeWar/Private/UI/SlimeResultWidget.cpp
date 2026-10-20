// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeResultWidget.h"

#include "Flow/SlimeRunGameState.h"
#include "GameFramework/PlayerController.h"
#include "UI/SlimeResultCamera.h"
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
	bool bFinal = false;

	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const UWorld* World = PC->GetWorld())
		{
			if (const ASlimeRunGameState* RunState = World->GetGameState<ASlimeRunGameState>())
			{
				Result = RunState->GetRunResult();
				bFinal = RunState->GetRunState() == ESlimeRunState::Ended;
			}
		}
	}

	OnResultReady(Result);

	// Numbers overwrite safely; animation and sound must not, or they would play at level start
	// (this widget is created long before the run ends and NativeConstruct calls us once there).
	if (bFinal)
	{
		OnResultShown();
	}
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

UTexture* USlimeResultWidget::GetResultPhoto() const
{
	const APlayerController* PC = GetOwningPlayer();
	ASlimeResultCamera* Camera = PC ? ASlimeResultCamera::Get(PC->GetWorld()) : nullptr;
	return Camera ? Camera->GetPhoto() : nullptr;
}

bool USlimeResultWidget::HasNextLevel() const
{
	const USlimeUISubsystem* UI = USlimeUISubsystem::Get(this);
	return UI && UI->HasNextLevel();
}

bool USlimeResultWidget::RequestNextLevel()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		return UI->RequestNextLevel();
	}
	return false;
}

void USlimeResultWidget::RequestLevelSelect()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestLevelSelect();
	}
}
