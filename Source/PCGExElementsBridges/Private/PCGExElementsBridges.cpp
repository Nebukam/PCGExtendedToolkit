// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsBridges.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsBridgesModule"

void FPCGExElementsBridgesModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsBridgesModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsBridgesModule, PCGExElementsBridges)
