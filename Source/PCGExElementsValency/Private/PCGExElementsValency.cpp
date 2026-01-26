// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsValency.h"

#include "Clusters/PCGExClusterCache.h"
#include "Core/PCGExCachedOrbitalCache.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsValencyModule"

#undef LOCTEXT_NAMESPACE


void FPCGExElementsValencyModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();

	// Register cluster cache factories
	PCGExClusters::FClusterCacheRegistry::Get().Register(
		MakeShared<PCGExValency::FOrbitalCacheFactory>());
}

void FPCGExElementsValencyModule::ShutdownModule()
{
	// Unregister cluster cache factories
	PCGExClusters::FClusterCacheRegistry::Get().Unregister(
		PCGExValency::FOrbitalCacheFactory::CacheKey);

	IPCGExModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsValencyModule, PCGExElementsValency)
