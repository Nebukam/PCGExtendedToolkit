// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"AIModule",
				"NavigationSystem",
				"StaticMeshDescription",
				"MeshDescription",
				"Engine",
				"PCG",
				"PCGGeometryScriptInterop",
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
					"UnrealEd",
					"Projects",
					"DetailCustomizations"
				}
			);
		}
	}
}