// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/SlimeAudioConfig.h"

#include "AkAudioEvent.h"
#include "Core/SlimeWarLog.h"

namespace
{
	/** Everything under the Wwise staging root; monster_merge is the one exception (see below). */
	const TCHAR* SfxRoot = TEXT("/Game/WwiseAudio/Events/Default_Work_Unit/SFX/");

	TSoftObjectPtr<UAkAudioEvent> MakeEvent(const TCHAR* Path)
	{
		return TSoftObjectPtr<UAkAudioEvent>(FSoftObjectPath(Path));
	}

	TSoftObjectPtr<UAkAudioEvent> MakeSfx(const TCHAR* Relative)
	{
		return MakeEvent(*FString::Printf(TEXT("%s%s"), SfxRoot, Relative));
	}
}

USlimeAudioConfig::USlimeAudioConfig()
{
	// Defaults are real paths on purpose: the project must make sound before anyone remembers to
	// create the data asset. A DA_AudioConfig exists to repoint events, not to enable audio.
	WeaponFire = MakeSfx(TEXT("weapon/weapon_01.weapon_01"));
	WeaponEmpty = MakeSfx(TEXT("weapon/weapon_empty.weapon_empty"));
	WeaponReload = MakeSfx(TEXT("weapon/weapon_reload.weapon_reload"));

	GrenadeExplosion = MakeSfx(TEXT("weapon/weapon02.weapon02"));

	ScoutReport = MakeSfx(TEXT("other/scout_report.scout_report"));
	MonsterAttack = MakeSfx(TEXT("other/monster_attact.monster_attact"));
	MonsterMergeEnd = MakeSfx(TEXT("other/monster_merge_end.monster_merge_end"));
	ResourceCollect = MakeSfx(TEXT("other/resource_collect.resource_collect"));
	ResourceDeliver = MakeSfx(TEXT("other/resource_deliver.resource_deliver"));
	MonsterMove = MakeSfx(TEXT("other/monster_move.monster_move"));

	// Not a typo: the only event that lives at the staging root instead of under SFX/.
	MonsterMerge = MakeEvent(TEXT("/Game/WwiseAudio/monster_merge.monster_merge"));

	PlayerHit = MakeSfx(TEXT("player/player_hit.player_hit"));
	PlayerDeath = MakeSfx(TEXT("player/player_death.player_death"));
	PlayerHealth = MakeSfx(TEXT("player/player_health.player_health"));
	PlayerFootstep = MakeSfx(TEXT("player/player_footstep.player_footstep"));
	WeaponHit = MakeSfx(TEXT("weapon/weapon_hit.weapon_hit"));
}

UAkAudioEvent* USlimeAudioConfig::GetEvent(ESlimeAudioEvent InEvent) const
{
	const TSoftObjectPtr<UAkAudioEvent>* Field = nullptr;
	switch (InEvent)
	{
	case ESlimeAudioEvent::WeaponFire:       Field = &WeaponFire; break;
	case ESlimeAudioEvent::WeaponEmpty:      Field = &WeaponEmpty; break;
	case ESlimeAudioEvent::WeaponReload:     Field = &WeaponReload; break;
	case ESlimeAudioEvent::GrenadeExplosion: Field = &GrenadeExplosion; break;
	case ESlimeAudioEvent::ScoutReport:      Field = &ScoutReport; break;
	case ESlimeAudioEvent::MonsterAttack:    Field = &MonsterAttack; break;
	case ESlimeAudioEvent::MonsterMerge:     Field = &MonsterMerge; break;
	case ESlimeAudioEvent::MonsterMergeEnd:  Field = &MonsterMergeEnd; break;
	case ESlimeAudioEvent::ResourceCollect:  Field = &ResourceCollect; break;
	case ESlimeAudioEvent::ResourceDeliver:  Field = &ResourceDeliver; break;
	case ESlimeAudioEvent::MonsterMove:      Field = &MonsterMove; break;
	case ESlimeAudioEvent::PlayerHit:        Field = &PlayerHit; break;
	case ESlimeAudioEvent::PlayerDeath:      Field = &PlayerDeath; break;
	case ESlimeAudioEvent::PlayerHealth:     Field = &PlayerHealth; break;
	case ESlimeAudioEvent::PlayerFootstep:   Field = &PlayerFootstep; break;
	case ESlimeAudioEvent::WeaponHit:        Field = &WeaponHit; break;
	default: break;
	}

	if (!Field || Field->IsNull())
	{
		return nullptr;
	}

	UAkAudioEvent* Event = Field->LoadSynchronous();
	if (!Event)
	{
		const uint8 Key = static_cast<uint8>(InEvent);
		if (!WarnedMissing.Contains(Key))
		{
			WarnedMissing.Add(Key);
			UE_LOG(LogSlimeWar, Warning,
				TEXT("SlimeAudio: event %d points at '%s' but it failed to load."),
				static_cast<int32>(Key), *Field->ToString());
		}
	}

	return Event;
}
