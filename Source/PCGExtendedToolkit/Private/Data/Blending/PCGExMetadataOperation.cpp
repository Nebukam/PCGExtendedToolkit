// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExMetadataOperation.h"

void UPCGExMetadataOperation::PrepareForData(const UPCGPointData* InData)
{
	BaseAttribute = InData->Metadata->GetMutableAttribute(AttributeName);
}

bool UPCGExMetadataOperation::UseFinalize() const { return false; }
bool UPCGExMetadataOperation::UsePreparation() const { return false; }

void UPCGExMetadataOperation::PrepareOperation(const PCGMetadataEntryKey OutputKey) const
{
}

void UPCGExMetadataOperation::DoOperation(const PCGMetadataEntryKey OperandAKey, const PCGMetadataEntryKey OperandBKey, const PCGMetadataEntryKey OutputKey, const double Alpha) const
{
}

void UPCGExMetadataOperation::FinalizeOperation(const PCGMetadataEntryKey OutputKey, double Alpha) const
{
}

void UPCGExMetadataOperation::ResetToDefault(const PCGMetadataEntryKey OutputKey) const
{
}

#define PCGEX_SAO_PREPAREDATA(_TYPE, _NAME)\
void UPCGExBlend##_NAME##Base::ResetToDefault(PCGMetadataEntryKey OutputKey) const { if(Attribute->HasNonDefaultValue(OutputKey)){Attribute->SetValue(OutputKey, Attribute->GetValue(PCGDefaultValueKey));} }\
void UPCGExBlend##_NAME##Base::PrepareForData(const UPCGPointData* InData) {Super::PrepareForData(InData);	Attribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(BaseAttribute); }\
_TYPE UPCGExBlend##_NAME##Base::GetValue(const PCGMetadataAttributeKey& Key) const { return Attribute->GetValueFromItemKey(Key); };
PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_PREPAREDATA)

#undef PCGEX_SAO_PREPAREDATA
