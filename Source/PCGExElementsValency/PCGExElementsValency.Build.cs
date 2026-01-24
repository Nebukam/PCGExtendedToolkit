// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExElementsValency : ModuleRules
{
	public PCGExElementsValency(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = (Target.Configuration == UnrealTargetConfiguration.Shipping);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
				"PCGExCore",
				"PCGExProperties",
				"PCGExBlending",
				"PCGExFilters",
				"PCGExFoundations",
				"PCGExGraphs",
				"PCGExCollections",
				"PCGExElementsProbing",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
