// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

EPCGExDataBlendingType UPCGExSubPointsBlendOperation::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Copy;
}

void UPCGExSubPointsBlendOperation::PrepareForData(const UPCGExPointIO* InData)
{
	Super::PrepareForData(InData);
	PrepareForData(InData->Out, InData->Out);
}

void UPCGExSubPointsBlendOperation::PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	if (!InternalBlender) { InternalBlender = NewObject<UPCGExMetadataBlender>(); }
	InternalBlender->DefaultOperation = GetDefaultBlending();
	InternalBlender->PrepareForData(InPrimaryData, InSecondaryData, BlendingSettings.AttributesOverrides);
	PropertiesBlender.Init(BlendingSettings);
}

void UPCGExSubPointsBlendOperation::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	BlendSubPoints(StartPoint, EndPoint, SubPoints, PathInfos, InternalBlender);
}

void UPCGExSubPointsBlendOperation::BlendSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos, const UPCGExMetadataBlender* InBlender) const
{
}

UPCGExMetadataBlender* UPCGExSubPointsBlendOperation::CreateBlender(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData)
{
	UPCGExMetadataBlender* NewBlender = NewObject<UPCGExMetadataBlender>();
	NewBlender->DefaultOperation = GetDefaultBlending();
	NewBlender->PrepareForData(InPrimaryData, InSecondaryData, BlendingSettings.AttributesOverrides);
	return NewBlender;
}

void UPCGExSubPointsBlendOperation::BeginDestroy()
{
	if (InternalBlender) { InternalBlender->Flush(); }
	InternalBlender = nullptr;
	Super::BeginDestroy();
}
