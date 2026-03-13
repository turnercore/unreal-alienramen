using UnrealBuildTool;

public class TagContentResolver : ModuleRules
{
	public TagContentResolver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags",
			"StructUtils"
		});
	}
}
