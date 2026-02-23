using UnrealBuildTool;
using System.Collections.Generic;

public class AlienRamenEditorTarget : TargetRules
{
	public AlienRamenEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "AlienRamen", "AlienRamenEditor" } );
	}
}
