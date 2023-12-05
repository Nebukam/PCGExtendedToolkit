// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExDataBlendingAverage.h"

#include "Data/Blending/PCGExDataBlending.h"

#define PCGEX_SAO_CLASS(_TYPE, _NAME)\
bool UPCGExDataBlendingAverage##_NAME::GetRequiresPreparation() const { return true; }\
bool UPCGExDataBlendingAverage##_NAME::GetRequiresFinalization() const { return true; }\
void UPCGExDataBlendingAverage##_NAME::PrepareOperation(const PCGMetadataEntryKey InPrimaryOutputKey) const { ResetToDefault(InPrimaryOutputKey); }\
void UPCGExDataBlendingAverage##_NAME::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha)const {\
const _TYPE A = GetPrimaryValue(InPrimaryKey); const _TYPE B = GetSecondaryValue(InSecondaryKey);\
PrimaryAttribute->SetValue(InPrimaryOutputKey, bInterpolationAllowed ? PCGExDataBlending::Add##_NAME(A, B) : Alpha > 0.5 ? B : A); };\
void UPCGExDataBlendingAverage##_NAME::FinalizeOperation(const PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const {\
const _TYPE Val = GetPrimaryValue(InPrimaryOutputKey);\
PrimaryAttribute->SetValue(InPrimaryOutputKey, bInterpolationAllowed ? PCGExDataBlending::Div##_NAME(Val, Alpha) : Val); };

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_CLASS)

#undef PCGEX_SAO_CLASS
