// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExElementsValencyEditor : ModuleRules
{
	public PCGExElementsValencyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = (Target.Configuration == UnrealTargetConfiguration.Shipping);

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
				"PCGExProperties",
				"PCGExCoreEditor",
				"PCGExFoundations",
				"PCGExCollections",
				"PCGExElementsClusters",
				"PCGExElementsValency",
				"EditorFramework",
				"LevelEditor"
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
