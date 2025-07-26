// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkitEditor : ModuleRules
{
	public PCGExtendedToolkitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"PlacementMode",
				"GeometryScriptingCore",
				"RenderCore",
				"RHI",
				"PCG",
				"PCGEditor",
				"PCGGeometryScriptInterop",
				"PCGExtendedToolkit"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"GeometryCore",
				"GeometryFramework",
				
				"GeometryAlgorithms",
				
				
				"PhysicsCore",
				"NavigationSystem",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"PropertyPath",
				
				"AppFramework",
				"ApplicationCore",
				"AssetDefinition",
				"AssetTools",
				"AssetRegistry",
				"BlueprintGraph",
				"ContentBrowser",
				"DesktopWidgets",
				"DetailCustomizations",
				"DeveloperSettings",
				"EditorFramework",
				"EditorScriptingUtilities",
				"EditorStyle",
				"EditorSubsystem",
				"EditorWidgets",
				"GameProjectGeneration",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"KismetWidgets",
				"PCG",
				"PropertyEditor",
				"RenderCore",
				"Slate",
				"SlateCore",
				"SourceControl",
				"StructUtilsEditor",
				"ToolMenus",
				"ToolWidgets",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UnrealEd",
				"LevelEditor",
				"SceneOutliner", 
				"AdvancedPreviewScene",
				"PCGEditor"
			}
		);
		
	}
}