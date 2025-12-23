// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsProbing.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExProbeFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsProbingModule"

void FPCGExElementsProbingModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsProbingModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsProbingModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Probe, Probe)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsProbingModule, PCGExElementsProbing)
