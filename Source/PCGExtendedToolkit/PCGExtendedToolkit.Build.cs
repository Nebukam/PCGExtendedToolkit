// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// if (Target.Platform == UnrealTargetPlatform.Win64) { bUseRTTI = true; }

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
			new string[]
			{
				"Core",
				"CoreUObject",
				"GeometryCore",
				"NavigationSystem",
				"Landscape",
				"Engine",
				"PCG"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		if (Target.bBuildEditor)
		{
			// Editor only modules
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					
				}
			);
		}
	}
}