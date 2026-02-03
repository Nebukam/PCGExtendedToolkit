// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.IO;
using UnrealBuildTool;

public class PCGExElementsTensors : ModuleRules
{
	public PCGExElementsTensors(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bNoPCH = File.Exists(Path.Combine(ModuleDirectory, "..", "..", "Config", ".noPCH")); 
		if (bNoPCH)                                                                    
		{                                                                                                                     
			PCHUsage = PCHUsageMode.NoPCHs;                                                                                   
		}                                                                                                                     
		else                                                                                                                  
		{                                                                                                                     
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;                                                                  
			PrivatePCHHeaderFile = "PCGExMinimalPCH.h";                                           
		} 
		
		bUseUnity = true;                                                                                                     
		MinSourceFilesForUnityBuildOverride = 4;
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
				"PCGExCore",
				"PCGExFilters",
				"PCGExHeuristics",
				"PCGExNoise3D",
				"PCGExFoundations",
				"PCGExBlending",
				"PCGExElementsProbing",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
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
