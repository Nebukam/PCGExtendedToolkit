// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExHeuristics.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExHeuristicsFactoryProvider.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExHeuristicsModule"

void FPCGExHeuristicsModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExHeuristicsModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExHeuristicsModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Heuristics, Heuristics)
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExHeuristicsModule, PCGExHeuristics)
