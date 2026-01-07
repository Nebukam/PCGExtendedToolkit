// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCollections.h"

#include "Core/PCGExAssetCollectionTypes.h"

#define LOCTEXT_NAMESPACE "FPCGExCollectionsModule"

void FPCGExCollectionsModule::StartupModule()
{
	// We need this because the registry holds a reference to the collection ::StaticClass
	// and it cannot be access during initialization so we defer it here.
	PCGExAssetCollection::FTypeRegistry::ProcessPendingRegistrations();
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExCollectionsModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExCollectionsModule, PCGExCollections)
