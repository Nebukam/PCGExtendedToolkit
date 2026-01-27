// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValencyEditor.h"

#include "EditorModeRegistry.h"
#include "PCGExAssetTypesMacros.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "Details/PCGExPropertyOutputConfigCustomization.h"
#include "Details/PCGExSocketCompatibilityCustomization.h"

void FPCGExElementsValencyEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	// Register Valency Cage editor mode
	FEditorModeRegistry::Get().RegisterMode<FPCGExValencyCageEditorMode>(
		FPCGExValencyCageEditorMode::ModeID,
		NSLOCTEXT("PCGExValency", "ValencyCageModeName", "PCGEx | Valency"),
		FSlateIcon(),
		true // Visible in mode toolbar
	);

	// Property customizations
	PCGEX_REGISTER_CUSTO_START
	PCGEX_REGISTER_CUSTO("PCGExValencyPropertyOutputConfig", FPCGExPropertyOutputConfigCustomization)
	PCGEX_REGISTER_CUSTO("PCGExSocketDefinition", FPCGExSocketDefinitionCustomization)
}

void FPCGExElementsValencyEditorModule::ShutdownModule()
{
	// Unregister editor mode
	FEditorModeRegistry::Get().UnregisterMode(FPCGExValencyCageEditorMode::ModeID);

	IPCGExEditorModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyEditorModule, PCGExElementsValencyEditor)
