// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValencyEditor.h"

#include "PCGExAssetTypesMacros.h"
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorMode/PCGExValencyCageSocketVisualizer.h"
#include "Components/PCGExValencyCageSocketComponent.h"
#include "Details/PCGExPropertyOutputConfigCustomization.h"
#include "Details/PCGExValencySocketCompatibilityCustomization.h"

void FPCGExElementsValencyEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	// UEdMode-based editor mode is auto-discovered via CDO - no FEditorModeRegistry needed

	// Register socket component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(
			UPCGExValencyCageSocketComponent::StaticClass()->GetFName(),
			MakeShareable(new FPCGExValencyCageSocketVisualizer()));
	}

	// Property customizations
	PCGEX_REGISTER_CUSTO_START
	PCGEX_REGISTER_CUSTO("PCGExValencyPropertyOutputConfig", FPCGExPropertyOutputConfigCustomization)
	PCGEX_REGISTER_CUSTO("PCGExValencySocketDefinition", FPCGExValencySocketDefinitionCustomization)
}

void FPCGExElementsValencyEditorModule::ShutdownModule()
{
	// Unregister socket component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UPCGExValencyCageSocketComponent::StaticClass()->GetFName());
	}

	// UEdMode-based editor mode is auto-discovered via CDO - no unregister needed

	IPCGExEditorModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyEditorModule, PCGExElementsValencyEditor)
