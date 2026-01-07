// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

using UnrealBuildTool;

public class PCGExElementsClipper2 : ModuleRules
{
	public PCGExElementsClipper2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine", 
				"PCG",
				"PCGExCore",
				"PCGExBlending",
				"PCGExFilters",
				"PCGExMatching",
				"PCGExFoundations",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
