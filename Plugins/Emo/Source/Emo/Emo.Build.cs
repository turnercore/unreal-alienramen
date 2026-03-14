using UnrealBuildTool;

public class Emo : ModuleRules
{
	public Emo(ReadOnlyTargetRules Target) : base(Target)
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
			"TagContentResolver"
		});
	}
}
