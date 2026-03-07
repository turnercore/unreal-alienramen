using UnrealBuildTool;
using System.IO;

public class AlienRamen : ModuleRules
{
	public AlienRamen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "GameplayTags", "GameplayAbilities","GameplayTasks", "AIModule", "NavigationSystem", "StateTreeModule", "GameplayStateTreeModule", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Stage Steam redistributables from tracked project SDK location.
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

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
