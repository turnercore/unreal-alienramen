using UnrealBuildTool;

public class ParleyEditor : ModuleRules
{
	public ParleyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"LevelEditor",
			"GraphEditor",
			"PropertyEditor",
			"ToolMenus",
			"GameplayTags",
			"GameplayTagsEditor",
			"DeveloperSettings",
			"TagContentResolver",
			"TagContentResolverEditor",
			"Parley"
		});
	}
}
