// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlimeWar : ModuleRules
{
	public SlimeWar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			// GAS（分层方案：玩家全量 / 敌人不挂 ASC）
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			// USlimeGameSettings : UDeveloperSettings
			"DeveloperSettings"
		});

		if (Target.bBuildEditor)
		{
			// GAS 编辑器支持（蓝图节点 / 资产面板），UncookedOnly
			PrivateDependencyModuleNames.Add("GameplayAbilitiesEditor");
		}
	}
}
