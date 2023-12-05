// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlendingOperation.h"

void UPCGExDataBlendingOperation::PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	PrimaryBaseAttribute = InPrimaryData->Metadata->GetMutableAttribute(AttributeName);
	SecondaryBaseAttribute = InSecondaryData->Metadata->GetMutableAttribute(AttributeName);

	if (!PrimaryBaseAttribute && SecondaryBaseAttribute)
	{
		PrimaryBaseAttribute = InPrimaryData->Metadata->CopyAttribute(SecondaryBaseAttribute, AttributeName, false, false, false);
	}

	bInterpolationAllowed = PrimaryBaseAttribute->AllowsInterpolation() && !SecondaryBaseAttribute->AllowsInterpolation();
	StrongTypeAttributes();
}

bool UPCGExDataBlendingOperation::GetRequiresFinalization() const { return false; }
bool UPCGExDataBlendingOperation::GetRequiresPreparation() const { return false; }

void UPCGExDataBlendingOperation::PrepareOperation(const PCGMetadataEntryKey InPrimaryOutputKey) const
{
}

void UPCGExDataBlendingOperation::DoOperation(const PCGMetadataEntryKey InPrimaryKey, const PCGMetadataEntryKey InSecondaryKey, const PCGMetadataEntryKey InPrimaryOutputKey, const double Alpha) const
{
}

void UPCGExDataBlendingOperation::FinalizeOperation(const PCGMetadataEntryKey InPrimaryOutputKey, double Alpha) const
{
}

void UPCGExDataBlendingOperation::ResetToDefault(const PCGMetadataEntryKey InPrimaryOutputKey) const
{
}

void UPCGExDataBlendingOperation::StrongTypeAttributes()
{
}

void UPCGExDataBlendingOperation::Flush()
{
	PrimaryBaseAttribute = nullptr;
	SecondaryBaseAttribute = nullptr;
}

#define PCGEX_SAO_PREPAREDATA(_TYPE, _NAME)\
void UPCGExBlend##_NAME##Base::ResetToDefault(PCGMetadataEntryKey InPrimaryOutputKey) const { if(PrimaryBaseAttribute->HasNonDefaultValue(InPrimaryOutputKey)){PrimaryAttribute->SetValue(InPrimaryOutputKey, PrimaryAttribute->GetValue(PCGDefaultValueKey));} }\
void UPCGExBlend##_NAME##Base::StrongTypeAttributes() {\
PrimaryAttribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(PrimaryBaseAttribute);\
SecondaryAttribute = static_cast<FPCGMetadataAttribute<_TYPE>*>(SecondaryBaseAttribute); }\
_TYPE UPCGExBlend##_NAME##Base::GetPrimaryValue(const PCGMetadataAttributeKey& Key) const { return PrimaryAttribute->GetValueFromItemKey(Key); };\
_TYPE UPCGExBlend##_NAME##Base::GetSecondaryValue(const PCGMetadataAttributeKey& Key) const { return SecondaryAttribute->GetValueFromItemKey(Key); };\
void UPCGExBlend##_NAME##Base::Flush(){Super::Flush(); PrimaryAttribute = nullptr; SecondaryAttribute = nullptr;}

PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_PREPAREDATA)

#undef PCGEX_SAO_PREPAREDATA
