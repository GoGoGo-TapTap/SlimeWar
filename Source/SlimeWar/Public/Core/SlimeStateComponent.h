// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SlimeStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlimeStateTagChangedSignature, FGameplayTag, Tag, bool, bAdded);

/**
 * Lightweight tag based state for objects that must NOT pay for an AbilitySystemComponent
 * (every enemy in this project). Also the single state query path for the player.
 *
 * The outward interface (OnStateTagChanged) matches what an ASC would expose, so a single
 * enemy can later be upgraded to a real ASC without touching the rest of the project.
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class USlimeStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeStateComponent();

	UFUNCTION(BlueprintCallable, Category = "Slime|State")
	void AddStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Slime|State")
	void RemoveStateTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Slime|State")
	bool HasStateTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "Slime|State")
	FGameplayTagContainer GetStateTags() const { return StateTags; }

	/** The only state change exit point. Subscribed by UI, debug HUD and the gameplay debugger. */
	UPROPERTY(BlueprintAssignable, Category = "Slime|State")
	FSlimeStateTagChangedSignature OnStateTagChanged;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Slime|State")
	FGameplayTagContainer StateTags;
};
