// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValencyEditor.h"

#include "PCGExAssetTypesMacros.h"
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorMode/PCGExValencyEditorModeToolkit.h"
#include "EditorMode/PCGExValencyCageConnectorVisualizer.h"
#include "Components/PCGExValencyCageConnectorComponent.h"
#include "Details/PCGExPropertyOutputConfigCustomization.h"
#include "Details/PCGExValencyConnectorCompatibilityCustomization.h"

void FPCGExElementsValencyEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	// Register editor mode command bindings
	FValencyEditorCommands::Register();

	// Register connector component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(
			UPCGExValencyCageConnectorComponent::StaticClass()->GetFName(),
			MakeShareable(new FPCGExValencyCageConnectorVisualizer()));
	}

	// Property customizations
	PCGEX_REGISTER_CUSTO_START
	PCGEX_REGISTER_CUSTO("PCGExValencyPropertyOutputConfig", FPCGExPropertyOutputConfigCustomization)
	PCGEX_REGISTER_CUSTO("PCGExValencyConnectorEntry", FPCGExValencyConnectorEntryCustomization)
}

void FPCGExElementsValencyEditorModule::ShutdownModule()
{
	// Unregister connector component visualizer
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UPCGExValencyCageConnectorComponent::StaticClass()->GetFName());
	}

	// Unregister editor mode command bindings
	FValencyEditorCommands::Unregister();

	IPCGExEditorModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyEditorModule, PCGExElementsValencyEditor)
