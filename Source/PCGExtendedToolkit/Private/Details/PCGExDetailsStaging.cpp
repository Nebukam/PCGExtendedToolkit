// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsStaging.h"

#include "Data/PCGExPointElements.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"


PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExAssetDistributionIndexDetails, Index, int32, true, IndexSource, -1);
PCGEX_SETTING_VALUE_IMPL(FPCGExAssetDistributionDetails, Category, FName, CategoryInput, CategoryAttribute, Category);
