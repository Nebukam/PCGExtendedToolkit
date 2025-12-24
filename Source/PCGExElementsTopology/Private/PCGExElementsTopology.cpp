// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsTopology.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsTopologyModule"

void FPCGExElementsTopologyModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsTopologyModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsTopologyModule, PCGExElementsTopology)
