// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkit.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif
#include "PCGExGlobalSettings.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitModule"

void FPCGExtendedToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FPCGExtendedToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitModule, PCGExtendedToolkit)
