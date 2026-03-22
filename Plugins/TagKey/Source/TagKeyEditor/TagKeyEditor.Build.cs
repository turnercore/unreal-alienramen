using UnrealBuildTool;

public class TagKeyEditor : ModuleRules
{
	public TagKeyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Engine",
			"UnrealEd",
			"TagKey"
		});
	}
}
