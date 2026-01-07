// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCoreEditor.h"

#define LOCTEXT_NAMESPACE "FPCGExCoreEditorModule"

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExCoreEditorModule, PCGExCoreEditor)

void FPCGExCoreEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();
}

void FPCGExCoreEditorModule::ShutdownModule()
{
	IPCGExEditorModuleInterface::ShutdownModule();
}
