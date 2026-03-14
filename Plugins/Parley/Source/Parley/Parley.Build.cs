using UnrealBuildTool;

public class Parley : ModuleRules
{
	public Parley(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"Projects",
			"DeveloperSettings",
			"UMG",
			"StructUtils",
			"TagKey"
		});
	}
}
