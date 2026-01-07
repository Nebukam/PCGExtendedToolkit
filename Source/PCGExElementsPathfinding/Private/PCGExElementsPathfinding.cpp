// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsPathfinding.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsPathfindingModule"

void FPCGExElementsPathfindingModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsPathfindingModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsPathfindingModule, PCGExElementsPathfinding)
