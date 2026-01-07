// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFoundations.h"


#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Core/PCGExPointStates.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExFoundationsModule"

void FPCGExFoundationsModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExFoundationsModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExFoundationsModule, PCGExFoundations)
