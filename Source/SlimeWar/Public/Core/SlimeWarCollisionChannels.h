// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * Project collision channel ids.
 *
 * The channels themselves are declared as text in Config/DefaultEngine.ini
 * ([/Script/Engine.CollisionProfile] +DefaultChannelResponses=...). Declaring them here keeps
 * C++ free of magic "ECC_GameTraceChannel1" literals in more than one place, exactly like the
 * native gameplay tags work (no DefaultGameplayTags.ini, no magic strings in gameplay code).
 */
namespace SlimeCollisionChannels
{
	/**
	 * Object channel used by every normal slime capsule (name: "SlimeFusion").
	 *
	 * Why a dedicated channel: a normal slime must be able to overlap its fusion partner
	 * (PB-10/PB-11 need real contact) while still blocking the world, the player and the
	 * aggressive slimes (PB-16). Channel level responses cannot express "only this pair",
	 * so the channel sets the default (Block) and the pairing window drops it to overlap
	 * with UPrimitiveComponent::IgnoreActorWhenMoving.
	 *
	 * Anything that queries enemies by object type must include this channel; the aim assist
	 * overlap query in ASlimeWarCharacter does.
	 */
	constexpr ECollisionChannel Fusion = ECC_GameTraceChannel1;
}
