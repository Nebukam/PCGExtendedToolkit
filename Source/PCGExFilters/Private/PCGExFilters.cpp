// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFilters.h"


#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "PCGEditorSettings.h"
#include "Core/PCGExPointFilter.h"
#include "Core/PCGExClusterFilter.h"
#include "Core/PCGExPointStates.h"
#endif


#define LOCTEXT_NAMESPACE "FPCGExFiltersModule"

void FPCGExFiltersModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExFiltersModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExFiltersModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(Filter, Filter, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterPoint, FilterPoint, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterCollection, FilterCollection, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterCluster, FilterCluster, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterVtx, FilterVtx, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(FilterEdge, FilterEdge, GetDefault<UPCGEditorSettings>()->FilterNodeColor)
	PCGEX_REGISTER_DATA_TYPE(PointState, PointState)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExFiltersModule, PCGExFilters)
