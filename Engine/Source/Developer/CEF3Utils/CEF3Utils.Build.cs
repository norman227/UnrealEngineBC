// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CEF3Utils : ModuleRules
{
	public CEF3Utils(TargetInfo Target)
	{
		PublicIncludePaths.Add("Developer/CEF3Utils/Public");
		PrivateIncludePaths.Add("Developer/CEF3Utils/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddThirdPartyPrivateStaticDependencies(Target,
				"CEF3"
				);
		}
	}
}
