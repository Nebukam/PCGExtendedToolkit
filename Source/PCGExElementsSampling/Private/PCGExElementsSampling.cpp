// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsSampling.h"


#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Core/PCGExTexParamFactoryProvider.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsSamplingModule"

void FPCGExElementsSamplingModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsSamplingModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
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
