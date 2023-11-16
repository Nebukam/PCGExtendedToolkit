// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "StaticMeshDescription",
                "MeshDescription",
                "Engine",
				"PCG",
                "PCGGeometryScriptInterop",
                "UnrealEd"
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
	}
}
