// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCore.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "AssetTypeActions_Base.h"
#include "Data/Bitmasks/PCGExBitmaskCollection.h"
#include "PCGExCoreEditor/Public/PCGExAssetTypesMacros.h"
#endif

IMPLEMENT_MODULE(FPCGExCoreModule, PCGExCore)

#if WITH_EDITOR
void FPCGExCoreModule::RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry)
{
	IPCGExModuleInterface::RegisterToEditor(InStyle, InRegistry);
	
	PCGEX_ASSET_TYPE_ACTION_BASIC(
		Bitmasks, "PCGEx Bitmasks", UPCGExBitmaskCollection,
		FColor(195, 0, 40), EAssetTypeCategories::Misc)
	
}
#endif
