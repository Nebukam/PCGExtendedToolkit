// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsSpatial.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsSpatialModule"

void FPCGExElementsSpatialModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsSpatialModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsSpatialModule, PCGExElementsSpatial)
