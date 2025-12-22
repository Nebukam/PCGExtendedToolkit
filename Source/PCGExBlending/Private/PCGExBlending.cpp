// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExBlending.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExBlendOpFactory.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExBlendingModule"

void FPCGExBlendingModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExBlendingModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExBlendingModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(BlendOp, BlendOp)
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExBlendingModule, PCGExBlending)
