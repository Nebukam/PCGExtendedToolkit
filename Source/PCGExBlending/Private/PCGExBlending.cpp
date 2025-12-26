// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExBlending.h"


#if WITH_EDITOR
#include "Core/PCGExBlendOpFactory.h"

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#endif

#define LOCTEXT_NAMESPACE "FPCGExBlendingModule"

void FPCGExBlendingModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExBlendingModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExBlendingModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION	
	PCGEX_REGISTER_DATA_TYPE(BlendOp, BlendOp)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExBlendingModule, PCGExBlending)
