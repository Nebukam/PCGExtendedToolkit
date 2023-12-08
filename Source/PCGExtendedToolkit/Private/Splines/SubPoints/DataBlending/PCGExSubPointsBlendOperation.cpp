// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "..\..\..\..\Public\Data\PCGExPointsIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Copy;
}

void UPCGExSubPointsBlendOperation::PrepareForData(PCGExData::FPointIO* InData)
{
	Super::PrepareForData(InData);
	PrepareForData(InData->GetOut(), InData->GetOut());
}

void UPCGExSubPointsBlendOperation::PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	if (InternalBlender) { delete InternalBlender; }
	InternalBlender = CreateBlender(InPrimaryData, InSecondaryData);
	PropertiesBlender.Init(BlendingSettings);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	BlendSubPoints(Start, End, SubPoints, PathInfos, InternalBlender);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(
	const PCGEx::FPointRef& StartPoint,
	const PCGEx::FPointRef& EndPoint,
	TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathInfos& PathInfos,
	const PCGExDataBlending::FMetadataBlender* InBlender) const
{
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendOperation::CreateBlender(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(GetDefaultBlending());
	NewBlender->PrepareForData(InPrimaryData, InSecondaryData, BlendingSettings.AttributesOverrides);
	return NewBlender;
}

void UPCGExSubPointsBlendOperation::BeginDestroy()
{
	if (InternalBlender) { delete InternalBlender; }
	InternalBlender = nullptr;
	Super::BeginDestroy();
}
