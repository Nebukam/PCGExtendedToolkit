// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsMeta.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#include "Elements/Partition/PCGExModularPartitionByValues.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsMetaModule"

void FPCGExElementsMetaModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsMetaModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsMetaModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	PCGEX_REGISTER_DATA_TYPE(PartitionRule, PartitionRule)
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExElementsMetaModule, PCGExElementsMeta)
