// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimePauseWidget.h"

#include "UI/SlimeUISubsystem.h"

void USlimePauseWidget::Resume()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->Resume();
	}
}

void USlimePauseWidget::RequestRetry()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestRetry();
	}
}

void USlimePauseWidget::RequestReselectDropPoint()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestReselectDropPoint();
	}
}

void USlimePauseWidget::RequestQuit()
{
	if (USlimeUISubsystem* UI = USlimeUISubsystem::Get(this))
	{
		UI->RequestQuit();
	}
}
