// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExFoundationsEditor : ModuleRules
{
	public PCGExFoundationsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"UnrealEd",
				"Settings",
				"Engine",
				"PCG",
				"PCGExCore",
				"PCGExCoreEditor",
				"PCGExFoundations"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PropertyPath",
				"DeveloperSettings",
				"Slate",
				"SlateCore",
				"PropertyEditor",
				"EditorWidgets",
				"InputCore",
				"ToolMenus" 
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					
				});
		}
	}
}
