// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExNoise3D.h"

#if WITH_EDITOR
#include "Styling/AppStyle.h"

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Core/PCGExNoise3DFactoryProvider.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExNoise3DModule"

void FPCGExNoise3DModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExNoise3DModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExNoise3DModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION
	PCGEX_REGISTER_DATA_TYPE(Noise3D, Noise3D)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExNoise3DModule, PCGExNoise3D)
