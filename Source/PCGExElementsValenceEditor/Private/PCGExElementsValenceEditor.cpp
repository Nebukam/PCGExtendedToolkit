// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValenceEditor.h"

void FPCGExElementsValenceEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	// Property customizations will be registered here
	// PCGEX_REGISTER_CUSTO_START
	// PCGEX_REGISTER_CUSTO("StructName", FCustomizationClass)
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValenceEditorModule, PCGExElementsValenceEditor)
