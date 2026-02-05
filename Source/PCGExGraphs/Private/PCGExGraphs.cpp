// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExGraphs.h"

#include "Clusters/PCGExClusterCache.h"
#include "Clusters/Artifacts/PCGExCachedFaceEnumerator.h"
#include "Clusters/Artifacts/PCGExCachedChain.h"

#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Core/PCGExPointStates.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExGraphsModule"

void FPCGExGraphsModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();

	// Register cluster cache factories
	PCGExClusters::FClusterCacheRegistry::Get().Register(
		MakeShared<PCGExClusters::FFaceEnumeratorCacheFactory>());
	PCGExClusters::FClusterCacheRegistry::Get().Register(
		MakeShared<PCGExClusters::FChainCacheFactory>());
}

void FPCGExGraphsModule::ShutdownModule()
{
	// Unregister cluster cache factories
	PCGExClusters::FClusterCacheRegistry::Get().Unregister(
		PCGExClusters::FFaceEnumeratorCacheFactory::CacheKey);
	PCGExClusters::FClusterCacheRegistry::Get().Unregister(
		PCGExClusters::FChainCacheFactory::CacheKey);

	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExGraphsModule, PCGExGraphs)
