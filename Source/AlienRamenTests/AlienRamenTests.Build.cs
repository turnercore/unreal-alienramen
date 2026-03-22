using UnrealBuildTool;

public class AlienRamenTests : ModuleRules
{
	public AlienRamenTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject",
			"Engine",
			"AlienRamen",
			"GameplayTags",
			"GameplayAbilities",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"DeveloperSettings",
			"TagKey",
			"Parley",
			"Emo",
			"Slate",
			"SlateCore"
		});
	}
}
