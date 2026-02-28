using UnrealBuildTool;

public class AlienRamenTests : ModuleRules
{
	public AlienRamenTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AlienRamen",
			"GameplayTags",
			"GameplayAbilities",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
