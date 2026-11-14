// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ParryAction : ModuleRules
{
	public ParryAction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Niagara" });

		PublicIncludePaths.AddRange(new string[] {
			"ParryAction",
			"ParryAction/Variant_Platforming",
			"ParryAction/Variant_Platforming/Animation",
			"ParryAction/Variant_Combat",
			"ParryAction/Variant_Combat/AI",
			"ParryAction/Variant_Combat/Animation",
			"ParryAction/Variant_Combat/Gameplay",
			"ParryAction/Variant_Combat/Interfaces",
			"ParryAction/Variant_Combat/UI",
			"ParryAction/Variant_SideScrolling",
			"ParryAction/Variant_SideScrolling/AI",
			"ParryAction/Variant_SideScrolling/Gameplay",
			"ParryAction/Variant_SideScrolling/Interfaces",
			"ParryAction/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
