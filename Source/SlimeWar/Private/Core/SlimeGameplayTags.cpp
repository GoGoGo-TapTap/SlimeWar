// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/SlimeGameplayTags.h"

// Player / weapon state
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Deploying, "State.Player.Deploying");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Controllable, "State.Player.Controllable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Dead, "State.Player.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Result, "State.Player.Result");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Invulnerable, "State.Player.Invulnerable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Weapon_Reloading, "State.Weapon.Reloading");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Weapon_Cooldown, "State.Weapon.Cooldown");

// Enemy AI state
// Reserved
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Stunned, "State.Player.Stunned");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Shielded, "State.Player.Shielded");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Debuff_Burning, "State.Enemy.Debuff.Burning");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Debuff_Slowed, "State.Enemy.Debuff.Slowed");

// SetByCaller magnitudes
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_FireCooldown, "Data.FireCooldown");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HitProtectionDuration, "Data.HitProtectionDuration");
