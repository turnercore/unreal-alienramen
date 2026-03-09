using UnrealBuildTool;

public class AlienRamenEditor : ModuleRules
{
	public AlienRamenEditor(ReadOnlyTargetRules Target) : base(Target)
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
			"AssetRegistry",
			"DeveloperSettings",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTagsEditor",
			"AlienRamen"
		});
	}
}
