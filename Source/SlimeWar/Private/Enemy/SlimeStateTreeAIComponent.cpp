// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeStateTreeAIComponent.h"

void USlimeStateTreeAIComponent::SetStateTreeAsset(UStateTree* InStateTree)
{
	// StateTreeRef is protected in UStateTreeComponent: only a derived class may set it.
	StateTreeRef.SetStateTree(InStateTree);
}
