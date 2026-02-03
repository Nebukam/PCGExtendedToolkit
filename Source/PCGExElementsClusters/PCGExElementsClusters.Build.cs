// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExElementsClusters : ModuleRules
{
	public PCGExElementsClusters(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "PCGExMinimalPCH.h";
		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 4;

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
				"Engine",
				"PCG",
				"PCGExCore", 
				"PCGExFilters",
				"PCGExBlending",
				"PCGExMatching",
				"PCGExHeuristics",
				"PCGExFoundations", 
				"PCGExGraphs", 
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryFramework",
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
					"Slate",
					"SlateCore"
				});
		}
	}
}
