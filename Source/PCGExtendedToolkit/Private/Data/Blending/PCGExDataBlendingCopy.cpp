// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingCopy.h"

#include "PCGExMath.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
void UPCGExDataBlendingCopy##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const{ PrimaryAttribute->SetValue(InPrimaryOutputKey, GetSecondaryValue(InSecondaryKey)); };

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_CLASS)

#undef PCGEX_SAO_CLASS
