// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;

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
		ToggleOptionalModule("PCGExElementsStates");
		ToggleOptionalModule("PCGExElementsTensors");
		ToggleOptionalModule("PCGExElementsTopology");
		
		ToggleOptionalModule("PCGExPathfinding");
		ToggleOptionalModule("PCGExPathfindingNavmesh");


		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				"ThirdParty/Delaunator/include"
			}
		);


		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GeometryScriptingCore",
				"PCG",
				"PCGGeometryScriptInterop",
				"Niagara",
				"PCGExCore",
				"PCGExFoundations",
				"PCGExBlending",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RenderCore",
				"RHI",
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms",
				"PhysicsCore",
				"NavigationSystem",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"PropertyPath",
				"DeveloperSettings",
				"PCGExCore",
				"PCGExFoundations"
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