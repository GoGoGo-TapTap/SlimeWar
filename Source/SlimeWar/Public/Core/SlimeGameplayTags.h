// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native gameplay tags.
 *
 * UE 5.5 rule: UE_DEFINE_GAMEPLAY_TAG can only be used in a .cpp file (it has a
 * static_assert on the file extension). This header only *declares* the tags with
 * UE_DECLARE_GAMEPLAY_TAG_EXTERN, the matching definitions live in
 * Private/Core/SlimeGameplayTags.cpp.
 *
 * Never create Config/DefaultGameplayTags.ini: that file would be a merge hotspot.
 */

// -- Player / weapon state --
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Deploying);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Controllable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Dead);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Result);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Invulnerable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Weapon_Reloading);
/** Granted by GE_FireCooldown while the weapon is between two shots (drives the fire rate). */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Weapon_Cooldown);

// -- Enemy AI state (driven by USlimeStateComponent, not GAS) --
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Normal_Idle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Normal_Fusing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Normal_MassLocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Aggro_Chasing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Aggro_WindingUp);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Aggro_Recovering);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Aggro_Cooling);

// -- Reserved, defined now so future work never has to change the contract --
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Stunned);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Shielded);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Debuff_Burning);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Debuff_Slowed);

// -- SetByCaller magnitude tags (rule 5: a GameplayEffect holds structure only) --
/** Damage amount handed to GE_Damage at apply time. */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage);
/** Fire cooldown duration handed to GE_FireCooldown at apply time. */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_FireCooldown);
/** Invulnerability duration handed to GE_Invulnerable at apply time. */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_HitProtectionDuration);
