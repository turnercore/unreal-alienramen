using UnrealBuildTool;

public class TagContentResolverEditor : ModuleRules
{
	public TagContentResolverEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"GameplayTags",
			"TagContentResolver"
		});
	}
}
