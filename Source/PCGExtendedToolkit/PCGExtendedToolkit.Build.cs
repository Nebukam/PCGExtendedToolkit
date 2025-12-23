// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;

		ToggleOptionalModule("PCGExCollections");
		ToggleOptionalModule("PCGExElementsActions");
		ToggleOptionalModule("PCGExElementsBridges");
		ToggleOptionalModule("PCGExElementsClusters");
		ToggleOptionalModule("PCGExElementsPaths");
		ToggleOptionalModule("PCGExElementsShapes");
		ToggleOptionalModule("PCGExElementsTensors");
		ToggleOptionalModule("PCGExElementsTopology");
		ToggleOptionalModule("PCGExElementsSampling");
		ToggleOptionalModule("PCGExElementsProbing");
		ToggleOptionalModule("PCGExElementsMeta");
		ToggleOptionalModule("PCGExElementsSpatial");
		ToggleOptionalModule("PCGExElementsPathfinding");
		ToggleOptionalModule("PCGExElementsPathfindingNavmesh");

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
				"PCGExCore",
				"PCGExBlending"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings"
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
					"UnrealEd",
					"Settings"
				});
		}
	}

	private void ToggleOptionalModule(string ModuleName, bool Enabled = true)
	{
		if (Enabled)
		{
			PublicDependencyModuleNames.Add(ModuleName);
			PublicDefinitions.Add($"PCGEX_{ModuleName.ToUpper()}_ENABLED=1");
		}
		else
		{
			PublicDefinitions.Add($"PCGEX_{ModuleName.ToUpper()}_ENABLED=0");
		}
	}
}