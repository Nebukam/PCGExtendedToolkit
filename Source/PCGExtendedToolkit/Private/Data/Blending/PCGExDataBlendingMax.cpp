// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingMax.h"

#include "Data/Blending/PCGExDataBlending.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
void UPCGExDataBlendingMax##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const {\
const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);\
 PrimaryAttribute->SetValue(InPrimaryOutputKey, PCGExDataBlending::Max##_NAME(A, B)); };

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_CLASS)

#undef PCGEX_SAO_CLASS
