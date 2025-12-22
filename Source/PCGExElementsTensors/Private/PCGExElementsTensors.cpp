// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsTensors.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsTensorsModule"

void FPCGExElementsTensorsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsTensorsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsTensorsModule, PCGExElementsTensors)
