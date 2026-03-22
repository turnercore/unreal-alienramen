using UnrealBuildTool;
using System.IO;
using System;

public class AlienRamen : ModuleRules
{
	public AlienRamen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"GameplayTags",
			"GameplayAbilities",
			"GameplayTasks",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"DeveloperSettings",
			"Parley",
			"Emo"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"NavigationSystem",
			"TagKey"
		});

		bool bDesktopTarget =
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Linux;

		if (bDesktopTarget)
		{
			DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		}

		const string SteamStageEnvName = "ALIENRAMEN_STAGE_STEAM_REDIST";
		bool bStageSteamRedistributables = string.Equals(Environment.GetEnvironmentVariable(SteamStageEnvName), "1", StringComparison.OrdinalIgnoreCase)
			|| string.Equals(Environment.GetEnvironmentVariable(SteamStageEnvName), "true", StringComparison.OrdinalIgnoreCase);
		if (bStageSteamRedistributables)
		{
			string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../"));
			string SteamRedistributableRoot = Path.Combine(ProjectRoot, "sdks", "Steamworks", "SteamvLatest");
			if (Directory.Exists(SteamRedistributableRoot))
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					RuntimeDependencies.Add("$(BinaryOutputDir)/steam_api64.dll", Path.Combine(SteamRedistributableRoot, "Win64", "steam_api64.dll"));
				}
				else if (Target.Platform == UnrealTargetPlatform.Mac)
				{
					RuntimeDependencies.Add("$(BinaryOutputDir)/libsteam_api.dylib", Path.Combine(SteamRedistributableRoot, "Mac", "libsteam_api.dylib"));
				}
				else if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					RuntimeDependencies.Add("$(BinaryOutputDir)/libsteam_api.so", Path.Combine(SteamRedistributableRoot, "x86_64-unknown-linux-gnu", "libsteam_api.so"));
				}
			}
		}
	}
}
