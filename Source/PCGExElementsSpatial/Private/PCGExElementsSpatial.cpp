// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsSpatial.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsTransformModule"

void FPCGExElementsSpatialModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsSpatialModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsSpatialModule, PCGExElementsTransform)
