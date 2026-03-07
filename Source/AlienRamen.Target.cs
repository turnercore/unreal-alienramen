using UnrealBuildTool;
using System.Collections.Generic;
using System;

public class AlienRamenTarget : TargetRules
{
	public AlienRamenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		string steamShippingId = Environment.GetEnvironmentVariable("UE_PROJECT_STEAMSHIPPINGID");
		if (string.IsNullOrWhiteSpace(steamShippingId))
		{
			steamShippingId = "480";
		}

		string steamProductName = Environment.GetEnvironmentVariable("UE_PROJECT_STEAMPRODUCTNAME");
		if (string.IsNullOrWhiteSpace(steamProductName))
		{
			steamProductName = steamShippingId;
		}

		string steamGameDir = Environment.GetEnvironmentVariable("UE_PROJECT_STEAMGAMEDIR");
		if (string.IsNullOrWhiteSpace(steamGameDir))
		{
			steamGameDir = "alienramen";
		}

		string steamGameDesc = Environment.GetEnvironmentVariable("UE_PROJECT_STEAMGAMEDESC");
		if (string.IsNullOrWhiteSpace(steamGameDesc))
		{
			steamGameDesc = "Alien Ramen";
		}

		GlobalDefinitions.Add("UE_PROJECT_STEAMPRODUCTNAME=\"" + steamProductName + "\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDIR=\"" + steamGameDir + "\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMGAMEDESC=\"" + steamGameDesc + "\"");
		GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=" + steamShippingId);

		ExtraModuleNames.AddRange( new string[] { "AlienRamen" } );
	}
}
