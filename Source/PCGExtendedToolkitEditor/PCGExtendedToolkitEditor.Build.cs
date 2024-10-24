// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkitEditor : ModuleRules
{
    public PCGExtendedToolkitEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "UnrealEd", 
                "Slate", 
                "SlateCore",
                "EditorStyle",
                "Projects"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );
    }
}