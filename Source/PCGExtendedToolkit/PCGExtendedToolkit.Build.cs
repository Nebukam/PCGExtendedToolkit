// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.IO;
using UnrealBuildTool;

public class PCGExtendedToolkit : ModuleRules
{
	public PCGExtendedToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = true;

		LoadSubModulesFromConfig();

		PublicIncludePaths.AddRange(
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
				"PCGExBlending"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings"
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
					"Settings",
					"PCGExCoreEditor",
					"PCGExFoundationsEditor"
				});
		}
	}

	private void LoadSubModulesFromConfig()
	{
		string SubModulesConfig = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "SubModules.ini"));

		// Register as external dependency - changes trigger rebuild
		ExternalDependencies.Add(SubModulesConfig);

		if (!File.Exists(SubModulesConfig))
		{
			return;
		}

		foreach (string Line in File.ReadAllLines(SubModulesConfig))
		{
			string Trimmed = Line.Trim();

			// Skip empty lines and comments
			if (string.IsNullOrEmpty(Trimmed) || Trimmed.StartsWith("#") || Trimmed.StartsWith(";"))
			{
				continue;
			}

			// Parse "ModuleName=1" or "ModuleName=0"
			string[] Parts = Trimmed.Split('=');
			if (Parts.Length == 2)
			{
				string ModuleName = Parts[0].Trim();
				bool Enabled = Parts[1].Trim() == "1";
				ToggleOptionalModule(ModuleName, Enabled);
			}
		}
	}

	private void ToggleOptionalModule(string ModuleName, bool Enabled)
	{
		if (!Enabled)
		{
			return;
		}

		PublicDependencyModuleNames.Add(ModuleName);

		if (Target.bBuildEditor == true)
		{
			// Check for companion editor module
			string EditorModuleName = ModuleName + "Editor";
			string EditorModulePath = Path.Combine(ModuleDirectory, "..", EditorModuleName);

			if (Directory.Exists(EditorModulePath))
			{
				PrivateDependencyModuleNames.Add(EditorModuleName);
			}
		}
	}
}