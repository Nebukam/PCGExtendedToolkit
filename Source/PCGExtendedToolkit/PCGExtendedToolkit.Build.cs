// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"GeometryScriptingCore",
				"RenderCore",
				"RHI",
				"PhysicsCore",
				"NavigationSystem"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		if (Target.bBuildEditor)
			// Editor only modules
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				}
			);
	}
}