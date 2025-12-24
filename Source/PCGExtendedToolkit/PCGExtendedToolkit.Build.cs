// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using UnrealBuildTool;
using EpicGames.Core;

public class PCGExtendedToolkit : ModuleRules
{
	private List<string> EnabledSubModules = new List<string>();

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
				"PCGExBlending",
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

		ScanSubModuleDependencies();
		UpdateUpluginFile();
	}

	// Read the small config file to know which modules are desirable
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

		// Track enabled PCGEx modules (excluding umbrella modules)
		if (ModuleName.StartsWith("PCGEx") &&
		    ModuleName != "PCGExtendedToolkit" &&
		    ModuleName != "PCGExtendedToolkitEditor")
		{
			EnabledSubModules.Add(ModuleName);
		}

		if (Target.bBuildEditor == true)
		{
			// Check for companion editor module
			string EditorModuleName = ModuleName + "Editor";
			string EditorModulePath = Path.Combine(ModuleDirectory, "..", EditorModuleName);

			if (Directory.Exists(EditorModulePath))
			{
				PrivateDependencyModuleNames.Add(EditorModuleName);

				// Track editor modules too
				if (EditorModuleName.StartsWith("PCGEx") &&
				    EditorModuleName != "PCGExtendedToolkitEditor")
				{
					EnabledSubModules.Add(EditorModuleName);
				}
			}
		}
	}

	private Dictionary<string, List<string>> ModuleDependencies = new Dictionary<string, List<string>>();

	private void ScanSubModuleDependencies()
	{
		foreach (string ModuleName in EnabledSubModules)
		{
			if (ModuleName.EndsWith("Editor") && !Target.bBuildEditor)
			{
				continue;
			}

			string BuildCsPath = Path.Combine(ModuleDirectory, "..", ModuleName, ModuleName + ".Build.cs");

			if (!File.Exists(BuildCsPath))
			{
				continue;
			}

			string Content = File.ReadAllText(BuildCsPath);
			List<string> Dependencies = new List<string>();

			// Match any "PCGEx..." strings in dependency arrays
			var Matches = System.Text.RegularExpressions.Regex.Matches(
				Content,
				@"""(PCGEx\w+)"""
			);

			foreach (System.Text.RegularExpressions.Match Match in Matches)
			{
				string Dep = Match.Groups[1].Value;

				// Don't add self-reference
				if (Dep != ModuleName && !Dependencies.Contains(Dep))
				{
					Dependencies.Add(Dep);
				}
			}

			ModuleDependencies[ModuleName] = Dependencies;
		}
	}

	#region Uplugin generation

	private void UpdateUpluginFile()
	{
		string UpluginPath = Path.Combine(ModuleDirectory, "..", "..", "PCGExtendedToolkit.uplugin");

		if (!File.Exists(UpluginPath))
		{
			return;
		}

		string Content = File.ReadAllText(UpluginPath);

		// Update Modules section (existing code)
		Content = UpdateModulesSection(Content);

		// Update Plugins section
		Content = UpdatePluginsSection(Content);

		File.WriteAllText(UpluginPath, Content);
	}

	private string UpdateModulesSection(string Content)
	{
		int ModulesStart = Content.IndexOf("\"Modules\"");
		if (ModulesStart == -1) return Content;

		int ArrayStart = Content.IndexOf('[', ModulesStart);
		if (ArrayStart == -1) return Content;

		int BracketCount = 1;
		int ArrayEnd = ArrayStart + 1;
		while (ArrayEnd < Content.Length && BracketCount > 0)
		{
			if (Content[ArrayEnd] == '[') BracketCount++;
			else if (Content[ArrayEnd] == ']') BracketCount--;
			ArrayEnd++;
		}

		HashSet<string> AllModules = new HashSet<string>(EnabledSubModules);
		foreach (var kvp in ModuleDependencies)
		{
			foreach (string Dep in kvp.Value)
			{
				if (Dep != "PCGExtendedToolkit" && Dep != "PCGExtendedToolkitEditor")
				{
					AllModules.Add(Dep);
				}
			}
		}

		// Check for Editor companions for all non-Editor modules
		if (Target.bBuildEditor)
		{
			HashSet<string> EditorModulesToAdd = new HashSet<string>();

			foreach (string ModuleName in AllModules)
			{
				if (ModuleName.EndsWith("Editor"))
				{
					continue;
				}

				string EditorModuleName = ModuleName + "Editor";
				string EditorModulePath = Path.Combine(ModuleDirectory, "..", EditorModuleName);

				if (Directory.Exists(EditorModulePath) && !AllModules.Contains(EditorModuleName))
				{
					EditorModulesToAdd.Add(EditorModuleName);
				}
			}

			foreach (string EditorModule in EditorModulesToAdd)
			{
				AllModules.Add(EditorModule);
			}
		}

		List<string> ModuleEntries = new List<string>();
		ModuleEntries.Add(BuildModuleEntry("PCGExtendedToolkit", false));
		ModuleEntries.Add(BuildModuleEntry("PCGExtendedToolkitEditor", true));

		foreach (string ModuleName in AllModules.OrderBy(m => m))
		{
			bool IsEditor = ModuleName.EndsWith("Editor");
			ModuleEntries.Add(BuildModuleEntry(ModuleName, IsEditor));
		}

		string ModulesJson = "[\n" + string.Join(",\n", ModuleEntries) + "\n  ]";

		return Content.Substring(0, ArrayStart) + ModulesJson + Content.Substring(ArrayEnd);
	}


	private string BuildModuleEntry(string ModuleName, bool IsEditor)
	{
		if (IsEditor)
		{
			return $@"    {{
		""Name"": ""{ModuleName}"",
		""Type"": ""Editor"", ""LoadingPhase"": ""Default"", ""PlatformAllowList"": [ ""Win64"", ""Mac"", ""Linux"" ]
    }}";
		}
		else
		{
			return $@"    {{
		""Name"": ""{ModuleName}"",
		""Type"": ""Runtime"", ""LoadingPhase"": ""Default"", ""PlatformAllowList"": [ ""Win64"", ""Mac"", ""IOS"", ""Android"", ""Linux"", ""LinuxArm64"" ]
	}}";
		}
	}

	private string UpdatePluginsSection(string Content)
	{
		int PluginsStart = Content.IndexOf("\"Plugins\"");
		if (PluginsStart == -1) return Content;

		var ArrayStart = Content.IndexOf('[', PluginsStart);
		if (ArrayStart == -1)
		{
			return Content;
		}

		int BracketCount = 1;
		int ArrayEnd = ArrayStart + 1;
		while (ArrayEnd < Content.Length && BracketCount > 0)
		{
			if (Content[ArrayEnd] == '[') BracketCount++;
			else if (Content[ArrayEnd] == ']') BracketCount--;
			ArrayEnd++;
		}

		// Always required
		List<string> pluginEntries = new List<string>();
		pluginEntries.Add(BuildPluginEntry("PCG"));

		HashSet<string> allModules = new HashSet<string>(EnabledSubModules);
		foreach (var kvp in ModuleDependencies)
		{
			foreach (string dep in kvp.Value)
			{
				allModules.Add(dep);
			}
		}

		// Conditional: only if a module needs GeometryScripting
		if (allModules.Overlaps(new[]
		    {
			    "PCGExElementsTopology",
		    }))
		{
			pluginEntries.Add(BuildPluginEntry("GeometryScripting"));
			pluginEntries.Add(BuildPluginEntry("PCGGeometryScriptInterop"));
		}

		string PluginsJson = "[\n" + string.Join(",\n", pluginEntries) + "\n  ]";

		return Content.Substring(0, ArrayStart) + PluginsJson + Content.Substring(ArrayEnd);
	}

	private string BuildPluginEntry(string PluginName)
	{
		return $@"    {{
      ""Name"": ""{PluginName}"",
      ""Enabled"": true
    }}";
	}

	#endregion
}