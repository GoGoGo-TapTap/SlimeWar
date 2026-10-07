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
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_Idle, "State.Enemy.Normal.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_Fusing, "State.Enemy.Normal.Fusing");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_MassLocked, "State.Enemy.Normal.MassLocked");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Chasing, "State.Enemy.Aggro.Chasing");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_WindingUp, "State.Enemy.Aggro.WindingUp");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Recovering, "State.Enemy.Aggro.Recovering");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Cooling, "State.Enemy.Aggro.Cooling");

// Reserved
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Stunned, "State.Player.Stunned");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Shielded, "State.Player.Shielded");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Debuff_Burning, "State.Enemy.Debuff.Burning");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Debuff_Slowed, "State.Enemy.Debuff.Slowed");

// SetByCaller magnitudes
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_FireCooldown, "Data.FireCooldown");
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HitProtectionDuration, "Data.HitProtectionDuration");
