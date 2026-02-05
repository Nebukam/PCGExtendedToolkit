// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsClustersRefine.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsClustersRefineModule"

void FPCGExElementsClustersRefineModule::StartupModule()
{
	OldBaseModules.Add(TEXT("PCGExElementsClusters"));
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsClustersRefineModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsClustersRefineModule, PCGExElementsClustersRefine)
