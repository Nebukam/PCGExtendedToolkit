// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsBridges.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsBridgesModule"

void FPCGExElementsBridgesModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsBridgesModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsBridgesModule, PCGExElementsBridges)
