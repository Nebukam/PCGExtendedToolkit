// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExModuleInterface.h"

void IPCGExModuleInterface::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	RegisteredModules.Add(this);
}

void IPCGExModuleInterface::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
	RegisteredModules.Remove(this);
}

#if WITH_EDITOR
void IPCGExModuleInterface::RegisterDataTypeInfos(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	
}
#endif
