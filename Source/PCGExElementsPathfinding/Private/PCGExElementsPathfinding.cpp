// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsPathfinding.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsPathfindingModule"

void FPCGExElementsPathfindingModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsPathfindingModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsPathfindingModule, PCGExElementsPathfinding)
