// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsShapes.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsShapesModule"

void FPCGExElementsShapesModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsShapesModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsShapesModule, PCGExElementsShapes)
