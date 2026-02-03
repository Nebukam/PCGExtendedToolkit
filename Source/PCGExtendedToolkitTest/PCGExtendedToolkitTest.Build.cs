// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkitTest : ModuleRules
{
	public PCGExtendedToolkitTest(ReadOnlyTargetRules Target) : base(Target)
	{
		// Disable PCH to reduce compiler memory pressure - test module is not performance-critical
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;                                                                                                     
		MinSourceFilesForUnityBuildOverride = 8;

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
				"PCGExCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PCGExFilters" // Only for filter enum tests
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
