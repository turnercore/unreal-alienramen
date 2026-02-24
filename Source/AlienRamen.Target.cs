using UnrealBuildTool;
using System.Collections.Generic;

public class AlienRamenTarget : TargetRules
{
	public AlienRamenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "AlienRamen" } );
	}
}
