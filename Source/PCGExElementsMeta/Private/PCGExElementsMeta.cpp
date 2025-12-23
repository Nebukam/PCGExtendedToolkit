// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsMeta.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsMetaModule"

void FPCGExElementsMetaModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsMetaModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsMetaModule, PCGExElementsMeta)
