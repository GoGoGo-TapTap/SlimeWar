// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/StatTableProvider.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Player/SlimeWeaponRules.h"
#include "Engine/DataTable.h"
#include "GameplayFramework/SlimeGameSettings.h"
#include "GameplayFramework/SlimeLevelDefinition.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	FName MakeSlimeRowName(int32 Mass)
	{
		return FName(*FString::FromInt(Mass));
	}
}

void UStatTableProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadTables();
}

void UStatTableProvider::Deinitialize()
{
	RunConfig = nullptr;
	SlimeStatTable = nullptr;
	WeaponStatTable = nullptr;
	AggroStatTable = nullptr;
	EcologyRules = nullptr;
	WeaponRules = nullptr;

	Super::Deinitialize();
}

bool UStatTableProvider::LoadTables()
{
	SlimeStatTable = nullptr;
	WeaponStatTable = nullptr;
	AggroStatTable = nullptr;
	EcologyRules = nullptr;
	WeaponRules = nullptr;
	RunConfig = nullptr;

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	if (!Settings)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("StatTableProvider: USlimeGameSettings is not available."));
		return false;
	}

	RunConfig = Settings->RunConfig.LoadSynchronous();
	if (!RunConfig)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("StatTableProvider: RunConfig is not set. Set it in Project Settings -> Game -> Slime War ")
			TEXT("(expected /Game/_SlimeWar/Core/Data/DA_RunConfig)."));
		return false;
	}

	LoadTablesFromRunConfig();

	return SlimeStatTable != nullptr && WeaponStatTable != nullptr;
}

void UStatTableProvider::LoadTablesFromRunConfig()
{
	if (!RunConfig)
	{
		SlimeStatTable = nullptr;
		WeaponStatTable = nullptr;
		AggroStatTable = nullptr;
		return;
	}

	SlimeStatTable = RunConfig->SlimeStatTable.LoadSynchronous();
	WeaponStatTable = RunConfig->WeaponStatTable.LoadSynchronous();
	AggroStatTable = RunConfig->AggroStatTable.LoadSynchronous();
	EcologyRules = RunConfig->EcologyRules.LoadSynchronous();
	WeaponRules = RunConfig->WeaponRules.LoadSynchronous();

	if (!EcologyRules)
	{
		EcologyRules = LoadObject<USlimeEcologyRules>(
			nullptr,
			TEXT("/Game/_SlimeWar/Ecology/DA_EcologyRules.DA_EcologyRules"));
	}

	if (!WeaponRules)
	{
		WeaponRules = LoadObject<USlimeWeaponRules>(
			nullptr,
			TEXT("/Game/_SlimeWar/Weapon/DA_Weapon_Rifle.DA_Weapon_Rifle"));
	}

	if (!WeaponRules)
	{
		WeaponRules = GetMutableDefault<USlimeWeaponRules>();
		UE_LOG(LogSlimeWar, Warning,
			TEXT("StatTableProvider: DA_Weapon_Rifle is missing; using USlimeWeaponRules defaults."));
	}

	if (!EcologyRules)
	{
		EcologyRules = GetMutableDefault<USlimeEcologyRules>();
		UE_LOG(LogSlimeWar, Warning,
			TEXT("StatTableProvider: DA_EcologyRules is missing; using USlimeEcologyRules defaults."));
	}

	if (!SlimeStatTable)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("StatTableProvider: SlimeStatTable is not set on %s."), *RunConfig->GetName());
	}

	if (!WeaponStatTable)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("StatTableProvider: WeaponStatTable is not set on %s."), *RunConfig->GetName());
	}

	if (!AggroStatTable)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("StatTableProvider: AggroStatTable is not set on %s (aggressive slimes fall back to defaults)."),
			*RunConfig->GetName());
	}

	UE_LOG(LogSlimeWar, Log,
		TEXT("StatTableProvider: loaded run config %s (slime table: %s, weapon table: %s, ecology rules: %s, weapon rules: %s)"),
		*RunConfig->GetName(),
		SlimeStatTable ? *SlimeStatTable->GetName() : TEXT("none"),
		WeaponStatTable ? *WeaponStatTable->GetName() : TEXT("none"),
		EcologyRules ? *EcologyRules->GetName() : TEXT("none"),
		WeaponRules ? *WeaponRules->GetName() : TEXT("none"));
}

bool UStatTableProvider::SelectLevel(FName LevelId)
{
	if (LevelId.IsNone())
	{
		// No level context (PIE opened a map directly, or the main menu world): keep whatever
		// LoadTables picked from the global settings.
		return false;
	}

	const USlimeGameSettings* Settings = GetDefault<USlimeGameSettings>();
	const USlimeLevelDefinition* Level = Settings ? Settings->FindLevelById(LevelId) : nullptr;

	if (!Level || Level->RunConfig.IsNull())
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("StatTableProvider: level '%s' has no RunConfig; keeping the current config."),
			*LevelId.ToString());
		return false;
	}

	USlimeRunConfig* NewRunConfig = Level->RunConfig.LoadSynchronous();
	if (!NewRunConfig)
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("StatTableProvider: RunConfig for level '%s' could not be loaded (%s); keeping the current config."),
			*LevelId.ToString(), *Level->RunConfig.ToString());
		return false;
	}

	if (NewRunConfig == RunConfig)
	{
		// Same config (a retry, or several levels sharing one tuning asset): nothing to reload.
		return true;
	}

	RunConfig = NewRunConfig;
	LoadTablesFromRunConfig();

	UE_LOG(LogSlimeWar, Log, TEXT("StatTableProvider: switched to the run config of level '%s' (%s)."),
		*LevelId.ToString(), *RunConfig->GetName());

	return true;
}

bool UStatTableProvider::ReloadTables()
{
	return LoadTables();
}

const USlimeEcologyRules* UStatTableProvider::GetEcologyRules() const
{
	return EcologyRules ? EcologyRules : GetDefault<USlimeEcologyRules>();
}

const USlimeWeaponRules* UStatTableProvider::GetWeaponRules() const
{
	return WeaponRules ? WeaponRules : GetDefault<USlimeWeaponRules>();
}

bool UStatTableProvider::GetSlimeStat(int32 Mass, FSlimeStatRow& Out) const
{
	if (!SlimeStatTable)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("GetSlimeStat(%d): slime stat table is not loaded, returning defaults."), Mass);
		return false;
	}

	const FName RowName = MakeSlimeRowName(Mass);
	const FSlimeStatRow* Row = SlimeStatTable->FindRow<FSlimeStatRow>(RowName, TEXT("GetSlimeStat"), false);
	if (!Row)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GetSlimeStat(%d): row '%s' was not found in %s, returning defaults."),
			Mass, *RowName.ToString(), *SlimeStatTable->GetName());
		return false;
	}

	Out = *Row;
	return true;
}

bool UStatTableProvider::GetWeaponStat(FName WeaponId, FWeaponStatRow& Out) const
{
	if (!WeaponStatTable)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("GetWeaponStat(%s): weapon stat table is not loaded, returning defaults."),
			*WeaponId.ToString());
		return false;
	}

	if (WeaponId.IsNone())
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("GetWeaponStat: weapon id is None, returning defaults."));
		return false;
	}

	const FWeaponStatRow* Row = WeaponStatTable->FindRow<FWeaponStatRow>(WeaponId, TEXT("GetWeaponStat"), false);
	if (!Row)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GetWeaponStat(%s): row was not found in %s, returning defaults."),
			*WeaponId.ToString(), *WeaponStatTable->GetName());
		return false;
	}

	Out = *Row;
	return true;
}

bool UStatTableProvider::GetAggroStat(FSlimeAggroStatRow& Out) const
{
	if (!AggroStatTable)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("GetAggroStat: aggro stat table is not loaded, returning defaults."));
		return false;
	}

	const FName RowName = RunConfig && !RunConfig->AggroRowName.IsNone() ? RunConfig->AggroRowName : FName(TEXT("Default"));
	const FSlimeAggroStatRow* Row = AggroStatTable->FindRow<FSlimeAggroStatRow>(RowName, TEXT("GetAggroStat"), false);
	if (!Row)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("GetAggroStat: row '%s' was not found in %s, returning defaults."),
			*RowName.ToString(), *AggroStatTable->GetName());
		return false;
	}

	Out = *Row;
	return true;
}

void UStatTableProvider::DumpLoadedTables() const
{
	UE_LOG(LogSlimeWar, Log, TEXT("==== Slime data tables ===="));
	UE_LOG(LogSlimeWar, Log, TEXT("RunConfig     : %s"), RunConfig ? *RunConfig->GetName() : TEXT("<null>"));

	const auto DumpTable = [](const UDataTable* Table, const TCHAR* Label)
	{
		if (!Table)
		{
			UE_LOG(LogSlimeWar, Error, TEXT("%s: <null> (not loaded, check DA_RunConfig)"), Label);
			return;
		}

		const TArray<FName> RowNames = Table->GetRowNames();

		UE_LOG(LogSlimeWar, Log, TEXT("%s: %s (%d rows) %s"),
			Label, *Table->GetName(), RowNames.Num(), *Table->GetPathName());

		for (const FName& RowName : RowNames)
		{
			UE_LOG(LogSlimeWar, Log, TEXT("    - %s"), *RowName.ToString());
		}
	};

	DumpTable(SlimeStatTable, TEXT("DT_SlimeStats "));
	DumpTable(WeaponStatTable, TEXT("DT_WeaponStats"));
	DumpTable(AggroStatTable, TEXT("DT_AggroStats  "));
	UE_LOG(LogSlimeWar, Log, TEXT("==========================="));
}
