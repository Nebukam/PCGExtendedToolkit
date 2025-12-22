// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsTopology.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

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

IMPLEMENT_MODULE(FPCGExElementsTopologyModule, PCGExElementsTopology)
