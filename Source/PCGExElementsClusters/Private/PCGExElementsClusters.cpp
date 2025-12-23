// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsClusters.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "Core/PCGExClusterStates.h"
#include "Data/Registry/PCGDataTypeRegistry.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsClustersModule"

void FPCGExElementsClustersModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsClustersModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsClustersModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
		
	PCGEX_REGISTER_DATA_TYPE(ClusterState, ClusterState)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsClustersModule, PCGExElementsClusters)
