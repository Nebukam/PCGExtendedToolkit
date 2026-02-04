// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsClustersRelax.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsClustersRelaxModule"

void FPCGExElementsClustersRelaxModule::StartupModule()
{
	OldBaseModules.Add(TEXT("PCGExElementsClusters"));
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsClustersRelaxModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsClustersRelaxModule, PCGExElementsClustersRelax)
