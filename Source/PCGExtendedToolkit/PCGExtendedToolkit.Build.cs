// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			bUseRTTI = true;
		}
		
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
				"Engine",
				"PCG",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Voronoi",
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