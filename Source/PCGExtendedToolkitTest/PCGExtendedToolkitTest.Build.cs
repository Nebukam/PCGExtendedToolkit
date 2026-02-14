// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.IO;
using UnrealBuildTool;

public class PCGExtendedToolkitTest : ModuleRules
{
	public PCGExtendedToolkitTest(ReadOnlyTargetRules Target) : base(Target)
	{
		// Disable PCH to reduce compiler memory pressure - test module is not performance-critical
		bool bNoPCH = File.Exists(Path.Combine(ModuleDirectory, "..", "..", "Config", ".noPCH")); 
		PCHUsage = bNoPCH ? PCHUsageMode.NoPCHs : PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;                                                                                                     
		MinSourceFilesForUnityBuildOverride = 4;
		PrecompileForTargets = PrecompileTargetsType.Any;

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
				"GeometryCore",
				"PCG",
				"PCGExCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PCGExBlending",
				"PCGExFilters",
				"PCGExFoundations",
				"PCGExProperties",
				"PCGExCollections",
				"PCGExGraphs",
				"PCGExElementsClustersDiagrams",
				"PCGExElementsSpatial",
				"PCGExMatching"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd"
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
