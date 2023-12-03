// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExSingleAttributeOperation.h"

void UPCGExSingleAttributeOperation::PrepareForData(UPCGPointData* InData)
{
	BaseAttribute = InData->Metadata->GetMutableAttribute(AttributeName);
}

bool UPCGExSingleAttributeOperation::UseFinalize() { return false; }

void UPCGExSingleAttributeOperation::DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha)
{
}

void UPCGExSingleAttributeOperation::FinalizeOperation(PCGMetadataEntryKey OutputKey, double Alpha)
{
}

void UPCGExSingleAttributeOperation::ResetToDefault(PCGMetadataEntryKey OutputKey)
{
}

#define PCGEX_SAO_PREPAREDATA(_TYPE, _NAME)\
void UPCGExBlend##_NAME##Base::ResetToDefault(PCGMetadataEntryKey OutputKey){ if(Attribute->HasNonDefaultValue(OutputKey)){Attribute->SetValue(OutputKey, Attribute->GetValue(PCGDefaultValueKey));} }\
void UPCGExBlend##_NAME##Base::PrepareForData(UPCGPointData* InData){Super::PrepareForData(InData);	Attribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(BaseAttribute); }\
_TYPE UPCGExBlend##_NAME##Base::GetValue(PCGMetadataAttributeKey Key){ return Attribute->GetValue(Key); };\

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_PREPAREDATA)

#undef PCGEX_SAO_PREPAREDATA