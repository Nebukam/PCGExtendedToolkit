// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValency.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsValencyModule"

#undef LOCTEXT_NAMESPACE


void FPCGExElementsValencyModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsValencyModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyModule, PCGExElementsValency)
