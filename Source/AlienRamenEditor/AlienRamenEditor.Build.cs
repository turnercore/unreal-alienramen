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
			"PropertyEditor",
			"ToolMenus",
			"AssetRegistry",
			"DeveloperSettings",
			"AlienRamen"
		});
	}
}
