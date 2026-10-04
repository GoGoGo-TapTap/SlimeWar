// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/SlimeStateComponent.h"

USlimeStateComponent::USlimeStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USlimeStateComponent::AddStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	// FGameplayTagContainer::AddTag returns void (it is a set), so check first.
	if (StateTags.HasTag(Tag))
	{
		return;
	}

	StateTags.AddTag(Tag);
	OnStateTagChanged.Broadcast(Tag, true);
}

void USlimeStateComponent::RemoveStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	const bool bRemoved = StateTags.RemoveTag(Tag);
	if (bRemoved)
	{
		OnStateTagChanged.Broadcast(Tag, false);
	}
}

bool USlimeStateComponent::HasStateTag(FGameplayTag Tag) const
{
	return StateTags.HasTag(Tag);
}
