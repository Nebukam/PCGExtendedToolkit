// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsPathfindingNavmesh.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsPathfindingNavmeshModule"

void FPCGExElementsPathfindingNavmeshModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsPathfindingNavmeshModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsPathfindingNavmeshModule, PCGExElementsPathfindingNavmesh)
