// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.IO;
using UnrealBuildTool;

public class PCGExProperties : ModuleRules
{
	public PCGExProperties(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bNoPCH = File.Exists(Path.Combine(ModuleDirectory, "..", "..", "Config", ".noPCH")); 
		PCHUsage = bNoPCH ? PCHUsageMode.NoPCHs : PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;                                                                                                     
		MinSourceFilesForUnityBuildOverride = 4;
		PrecompileForTargets = PrecompileTargetsType.Any; 

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

		// NOTE FOR CUSTOM PROPERTY TYPES IN OTHER MODULES:
		// If you define a custom FPCGExProperty derivative in another module,
		// that module must depend on PCGExProperties (for FPCGExProperty base class)
		// and PCGExCore (for PCGExData::TBuffer, FFacade, etc.).
		// The property will automatically appear in FInstancedStruct pickers
		// that are constrained to BaseStruct=FPCGExProperty.
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
				"PCGExCore",
				"StructUtils",
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

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"Slate",
					"SlateCore",
				});
		}
	}
}
