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
			"DeveloperSettings",
			// Phase A 敌人 AI：AIController + NavMesh 寻路
			"AIModule",
			"NavigationSystem",
			// Phase D UI：UUserWidget 基类 + ULocalPlayerSubsystem
			"UMG",
			"Slate",
			"SlateCore",
			// Phase D 演出：投放 / 结算的 Level Sequence 播放
			"LevelSequence",
			"MovieScene",
			// 音频：C++ 侧直接 PostEvent / StopPlayingID（事件资产统一在 DA_AudioConfig 里配）
			"AkAudio"
		});

		if (Target.bBuildEditor)
		{
			// GAS 编辑器支持（蓝图节点 / 资产面板），UncookedOnly
			PrivateDependencyModuleNames.Add("GameplayAbilitiesEditor");
		}
	}
}
