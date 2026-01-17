// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValence.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsValenceModule"

#undef LOCTEXT_NAMESPACE


void FPCGExElementsValenceModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsValenceModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValenceModule, PCGExElementsValence)
