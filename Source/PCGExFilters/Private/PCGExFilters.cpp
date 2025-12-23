// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFilters.h"

#include "PCGEditorSettings.h"
#include "Core/PCGExClusterFilter.h"
#include "Core/PCGExPointFilter.h"
#include "Data/Registry/PCGDataTypeRegistry.h"


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

	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(Filter, Filter, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterPoint, FilterPoint, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterCollection, FilterCollection, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterCluster, FilterCluster, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterVtx, FilterVtx, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterEdge, FilterEdge, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExFiltersModule, PCGExFilters)
