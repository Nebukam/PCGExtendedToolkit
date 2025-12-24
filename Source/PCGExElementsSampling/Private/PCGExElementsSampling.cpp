// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsSampling.h"


#if WITH_EDITOR
#include "Core/PCGExTexParamFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsSamplingModule"

void FPCGExElementsSamplingModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsSamplingModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsSamplingModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION
	PCGEX_REGISTER_DATA_TYPE(TexParam, TexParam)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsSamplingModule, PCGExElementsSampling)
