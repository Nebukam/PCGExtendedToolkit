// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsClusters.h"



#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Core/PCGExClusterStates.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsClustersModule"

void FPCGExElementsClustersModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsClustersModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsClustersModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_REGISTER_PIN_ICON(IN_Vtx)
	PCGEX_REGISTER_PIN_ICON(OUT_Vtx)
	
	PCGEX_REGISTER_PIN_ICON(IN_Edges)
	PCGEX_REGISTER_PIN_ICON(OUT_Edges)
	
	PCGEX_START_PCG_REGISTRATION	
	PCGEX_REGISTER_DATA_TYPE(ClusterState, ClusterState)
	PCGEX_REGISTER_DATA_TYPE(FillControl, FillControl)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsClustersModule, PCGExElementsClusters)
