using UnrealBuildTool;
using System.Collections.Generic;

public class AlienRamenTarget : TargetRules
{
	public AlienRamenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		GlobalDefinitions.Add("UE_PROJECT_STEAMPRODUCTNAME=\"480\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDIR=\"alienramen\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDESC=\"Alien Ramen\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=480");

		ExtraModuleNames.AddRange( new string[] { "AlienRamen" } );
	}
}
