// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.IO;
using UnrealBuildTool;

public class PCGExFoundations : ModuleRules
{
	public PCGExFoundations(ReadOnlyTargetRules Target) : base(Target)
	{
		bool bNoPCH = File.Exists(Path.Combine(ModuleDirectory, "..", "..", "Config", ".noPCH"));
		if (bNoPCH)
		{
			PCHUsage = PCHUsageMode.NoPCHs;
		}
		else
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			PrivatePCHHeaderFile = "Public/PCGExFoundationsPCH.h";
			SharedPCHHeaderFile = "Public/PCGExFoundationsPCH.h";
		}

		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 4;
		//IWYUSupport = IWYUSupport.Full;

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
				"PCGExCore",
				"PCGExBlending",
				"PCGExFilters",
				"PCGExPickers",
				"PCGExMatching",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryFramework",
				"PropertyPath"
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
