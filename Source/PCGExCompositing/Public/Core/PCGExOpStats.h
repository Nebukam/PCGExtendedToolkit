// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExDataFilter.h"
#include "Details/PCGExAttributesDetails.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataCommon.h"

#include "PCGExBlending.generated.h"

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FOpStats
	{
		int32 Count = 0;
		double TotalWeight = 0;
	};
}
