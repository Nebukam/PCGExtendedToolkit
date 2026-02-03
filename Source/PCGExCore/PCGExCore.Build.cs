// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

using System.IO;                                                                                                      

public class PCGExCore : ModuleRules
{
	public PCGExCore(ReadOnlyTargetRules Target) : base(Target)
	{
		// Set this to 0 once migration is complete
		PublicDefinitions.Add("PCGEX_SUBMODULE_CORE_REDIRECT_ENABLED=1");
		
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
		
		// Uncomment if you get PCH memory exhaustion errors (C3859/C1076):
		//PublicDefinitions.Add("PCGEX_FAT_PCH=0");
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
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms"
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
					"Settings",
					"Slate",
					"SlateCore",
					"ToolMenus"
				});
		}
	}
}
