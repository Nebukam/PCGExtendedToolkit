// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCollections.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif
#include "PCGExGlobalSettings.h"
#include "Core/PCGExAssetCollectionTypes.h"

#define LOCTEXT_NAMESPACE "FPCGExCollectionsModule"

void FPCGExCollectionsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	// We need this because the registry holds a reference to the collection ::StaticClass
	// and it cannot be access during initialization so we defer it here.
	PCGExAssetCollection::FTypeRegistry::ProcessPendingRegistrations();
}

void FPCGExCollectionsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExCollectionsModule, PCGExCollections)
