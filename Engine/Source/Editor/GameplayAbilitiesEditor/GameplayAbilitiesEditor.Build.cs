// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilitiesEditor : ModuleRules
	{
        public GameplayAbilitiesEditor(TargetInfo Target)
		{
            PrivateIncludePaths.AddRange(
                new string[] {
                    "Editor/GraphEditor/Private",
				    "Editor/Kismet/Private",
					"Editor/GameplayAbilitiesEditor/Private",
                    "Developer/AssetTools/Private",
				}
			);

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
                    "GameplayTags",
					"GameplayAbilities",
                    "InputCore",
                    "PropertyEditor",
					"Slate",
					"SlateCore",
                    "EditorStyle",
					"BlueprintGraph",
                    "Kismet",
					"KismetCompiler",
					"GraphEditor",
					"MainFrame",
					"UnrealEd",
				}
			);
		}
	}
}