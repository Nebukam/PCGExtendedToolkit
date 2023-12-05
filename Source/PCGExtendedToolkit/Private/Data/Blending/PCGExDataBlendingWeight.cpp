// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingWeight.h"

#include "Data/Blending/PCGExDataBlending.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
void UPCGExDataBlendingWeight##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const {\
const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);\
PrimaryAttribute->SetValue(InPrimaryOutputKey, bInterpolationAllowed ? PCGExDataBlending::Lerp##_NAME(A, B, Alpha) : Alpha > 0.5 ? B : A); };

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_CLASS)

#undef PCGEX_SAO_CLASS
