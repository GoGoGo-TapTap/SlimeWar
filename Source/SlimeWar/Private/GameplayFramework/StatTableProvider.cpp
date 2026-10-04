// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework/StatTableProvider.h"
#include "Core/SlimeWarLog.h"
#include "Engine/DataTable.h"
#include "GameplayFramework/SlimeGameSettings.h"
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

	Super::Deinitialize();
}

bool UStatTableProvider::LoadTables()
{
	SlimeStatTable = nullptr;
	WeaponStatTable = nullptr;
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

	SlimeStatTable = RunConfig->SlimeStatTable.LoadSynchronous();
	WeaponStatTable = RunConfig->WeaponStatTable.LoadSynchronous();

	if (!SlimeStatTable)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("StatTableProvider: SlimeStatTable is not set on %s."), *RunConfig->GetName());
	}

	if (!WeaponStatTable)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("StatTableProvider: WeaponStatTable is not set on %s."), *RunConfig->GetName());
	}

	UE_LOG(LogSlimeWar, Log, TEXT("StatTableProvider: loaded run config %s (slime table: %s, weapon table: %s)"),
		*RunConfig->GetName(),
		SlimeStatTable ? *SlimeStatTable->GetName() : TEXT("none"),
		WeaponStatTable ? *WeaponStatTable->GetName() : TEXT("none"));

	return SlimeStatTable != nullptr && WeaponStatTable != nullptr;
}

bool UStatTableProvider::ReloadTables()
{
	return LoadTables();
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
	UE_LOG(LogSlimeWar, Log, TEXT("==========================="));
}
