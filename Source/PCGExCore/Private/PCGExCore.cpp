// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCore.h"



#if WITH_EDITOR

#if PCGEX_ENGINE_VERSION > 506
#include "Data/Registry/PCGDataTypeRegistry.h" // PCGEX_PCG_DATA_REGISTRY
#endif

#include "Sorting/PCGExSortingRuleProvider.h"
#include "AssetTypeActions_Base.h"
#include "Data/Bitmasks/PCGExBitmaskCollection.h"
#include "PCGExCoreEditor/Public/PCGExAssetTypesMacros.h"
#endif

PCGEX_IMPLEMENT_MODULE(FPCGExCoreModule, PCGExCore)

#if WITH_EDITOR
void FPCGExCoreModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle);

	PCGEX_START_PCG_REGISTRATION	
	PCGEX_REGISTER_DATA_TYPE(SortRule, SortRule)

	PCGEX_REGISTER_PIN_ICON(OUT_Special)
	PCGEX_REGISTER_PIN_ICON(IN_Special)

	PCGEX_REGISTER_PIN_ICON(OUT_RecursionTracker)
	PCGEX_REGISTER_PIN_ICON(IN_RecursionTracker)
	
	PCGEX_ASSET_TYPE_ACTION_BASIC(
		Bitmasks, "PCGEx Bitmasks", UPCGExBitmaskCollection,
		FColor(195, 0, 40), EAssetTypeCategories::Misc)
}
#endif
