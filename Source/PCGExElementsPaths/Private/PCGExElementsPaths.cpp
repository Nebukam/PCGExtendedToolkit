// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsPaths.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsPathsModule"

void FPCGExElementsPathsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsPathsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsPathsModule, PCGExElementsPaths)
