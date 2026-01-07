// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsMeta.h"


#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Elements/Partition/PCGExModularPartitionByValues.h"
#include "AssetTypeActions_Base.h"
#include "Elements/PCGExPackActorData.h"
#include "PCGExCoreEditor/Public/PCGExAssetTypesMacros.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsMetaModule"

void FPCGExElementsMetaModule::StartupModule()
{
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsMetaModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#if WITH_EDITOR
void FPCGExElementsMetaModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION	
	PCGEX_REGISTER_DATA_TYPE(PartitionRule, PartitionRule)

	PCGEX_ASSET_TYPE_ACTION_BASIC(
		ActorDataPacker, "PCGEx Actor Data Packer", UPCGExCustomActorDataPacker,
		FColor(195, 124, 40), EAssetTypeCategories::Misc)
}
#endif

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsMetaModule, PCGExElementsMeta)
