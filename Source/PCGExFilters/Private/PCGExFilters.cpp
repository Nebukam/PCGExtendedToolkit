// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFilters.h"

#include "Core/PCGExClusterFilter.h"
#include "Core/PCGExPointFilter.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExFiltersModule"

void FPCGExFiltersModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExFiltersModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExFiltersModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_REGISTER_DATA_TYPE(Filter, Filter)
	PCGEX_REGISTER_DATA_TYPE(FilterPoint, FilterPoint)
	PCGEX_REGISTER_DATA_TYPE(FilterCollection, FilterCollection)
	PCGEX_REGISTER_DATA_TYPE(FilterCluster, FilterCluster)
	PCGEX_REGISTER_DATA_TYPE(FilterVtx, FilterVtx)
	PCGEX_REGISTER_DATA_TYPE(FilterEdge, FilterEdge)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExFiltersModule, PCGExFilters)
