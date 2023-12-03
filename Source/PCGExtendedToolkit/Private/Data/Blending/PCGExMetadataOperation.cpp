// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExMetadataOperation.h"

void UPCGExMetadataOperation::PrepareForData(const UPCGPointData* InData)
{
	PrimaryBaseAttribute = InData->Metadata->GetMutableAttribute(AttributeName);
	SecondaryBaseAttribute = PrimaryBaseAttribute;
	StrongTypeAttributes();
}

void UPCGExMetadataOperation::PrepareForData(const UPCGPointData* InData, const UPCGPointData* InOtherData)
{
	PrimaryBaseAttribute = InData->Metadata->GetMutableAttribute(AttributeName);
	SecondaryBaseAttribute = InOtherData->Metadata->GetMutableAttribute(AttributeName);
	StrongTypeAttributes();
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
void UPCGExBlend##_NAME##Base::ResetToDefault(PCGMetadataEntryKey OutputKey) const { if(PrimaryBaseAttribute->HasNonDefaultValue(OutputKey)){PrimaryAttribute->SetValue(OutputKey, PrimaryAttribute->GetValue(PCGDefaultValueKey));} }\
void UPCGExBlend##_NAME##Base::StrongTypeAttributes() {\
PrimaryAttribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(PrimaryBaseAttribute);\
SecondaryAttribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(SecondaryBaseAttribute); }\
_TYPE UPCGExBlend##_NAME##Base::GetPrimaryValue(const PCGMetadataAttributeKey& Key) const { return PrimaryAttribute->GetValueFromItemKey(Key); };\
_TYPE UPCGExBlend##_NAME##Base::GetSecondaryValue(const PCGMetadataAttributeKey& Key) const { return SecondaryAttribute->GetValueFromItemKey(Key); };

void UPCGExMetadataOperation::StrongTypeAttributes()
{
}

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_PREPAREDATA)

#undef PCGEX_SAO_PREPAREDATA
