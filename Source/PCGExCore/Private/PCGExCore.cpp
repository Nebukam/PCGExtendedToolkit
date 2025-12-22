// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCore.h"

#if WITH_EDITOR
#include "Data/Registry/PCGDataTypeRegistry.h"
#include "Sorting/PCGExSortingRuleProvider.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExCoreModule"

void FPCGExCoreModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExCoreModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExCoreModule::RegisterDataTypeInfos(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterDataTypeInfos(InStyle, InRegistry);

	PCGEX_REGISTER_DATA_TYPE(SortRule, SortRule)
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExCoreModule, PCGExCore)
