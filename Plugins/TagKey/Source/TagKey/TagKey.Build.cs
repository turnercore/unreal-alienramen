using UnrealBuildTool;

public class TagKey : ModuleRules
{
	public TagKey(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AssetRegistry"
		});
	}
}
