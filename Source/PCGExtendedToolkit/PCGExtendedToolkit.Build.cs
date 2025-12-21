// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;

public class PCGExtendedToolkit : ModuleRules
{
	private HashSet<string> AvailableModules;

	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;

		AvailableModules = LoadAvailableModules();


		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				"ThirdParty/Delaunator/include"
			}
		);


		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GeometryScriptingCore",
				"PCG",
				"PCGGeometryScriptInterop",
				"Niagara",
				"PCGExCore",
				"PCGExFoundations",
				"PCGExBlending",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RenderCore",
				"RHI",
				"GeometryCore",
				"GeometryFramework",
				"GeometryAlgorithms",
				"PhysicsCore",
				"NavigationSystem",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"PropertyPath",
				"DeveloperSettings",
				"PCGExCore",
				"PCGExFoundations"
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
					"Settings"
				});
		}

		TryAddOptionalModule("PCGExShapes");
		TryAddOptionalModule("PCGExActions");
		TryAddOptionalModule("PCGExTensors");
		TryAddOptionalModule("PCGExTopologies");
		TryAddOptionalModule("PCGExPathfinding");
		TryAddOptionalModule("PCGExPathfindingNavmesh");
		TryAddOptionalModule("PCGExBridges");
	}

	private HashSet<string> LoadAvailableModules()
	{
		var modules = new HashSet<string>();

		try
		{
			string PluginFile = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "PCGExtendedToolkit.uplugin"));
			FileReference PluginFileRef = new FileReference(PluginFile);

			// Use Unreal's built-in plugin descriptor parser
			PluginDescriptor Descriptor = PluginDescriptor.FromFile(PluginFileRef);

			if (Descriptor != null && Descriptor.Modules != null)
			{
				foreach (ModuleDescriptor Module in Descriptor.Modules)
				{
					modules.Add(Module.Name.ToString());
				}
			}
		}
		catch (Exception ex)
		{
			System.Console.WriteLine($"[PCGEx] Warning: Failed to load plugin descriptor: {ex.Message}");
		}

		return modules;
	}

	private void TryAddOptionalModule(string ModuleName)
	{
		if (AvailableModules.Contains(ModuleName))
		{
			PublicDependencyModuleNames.Add(ModuleName);
			PublicDefinitions.Add($"PCGEX_{ModuleName.ToUpper()}_ENABLED=1");
		}
		else
		{
			PublicDefinitions.Add($"PCGEX_{ModuleName.ToUpper()}_ENABLED=0");
		}
	}
}