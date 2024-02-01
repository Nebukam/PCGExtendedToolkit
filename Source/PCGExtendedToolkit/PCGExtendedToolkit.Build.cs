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
				"Projects", // So that we can use the IPluginManager, required for icons
				"DetailCustomizations",
				"PCG"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		// Include "UnrealEd" only for editor configuration
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}