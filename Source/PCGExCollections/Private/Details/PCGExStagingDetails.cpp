// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExStagingDetails.h"

#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Core/PCGExCollectionHelpers.h"
#include "Details/PCGExSettingsDetails.h"

FPCGExAssetDistributionIndexDetails::FPCGExAssetDistributionIndexDetails()
{
	if (IndexSource.GetName() == FName("@Last")) { IndexSource.Update(TEXT("$Index")); }
}

PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExAssetDistributionIndexDetails, Index, int32, true, IndexSource, -1);
PCGEX_SETTING_VALUE_IMPL(FPCGExAssetDistributionDetails, Category, FName, CategoryInput, CategoryAttribute, Category);
