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
			"Engine",
			"UnrealEd",
			"GameplayTags",
			"TagKey"
		});
	}
}
