// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExElementsTopology : ModuleRules
{
	public PCGExElementsTopology(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		//IWYUSupport = IWYUSupport.Full;

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
				"GeometryScriptingCore",
				"PCGGeometryScriptInterop",
				"PCGExCore",
				"PCGExFilters",
				"PCGExFoundations",
				"PCGExGraphs",
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
					"SlateCore",
				});
		}
	}
}
