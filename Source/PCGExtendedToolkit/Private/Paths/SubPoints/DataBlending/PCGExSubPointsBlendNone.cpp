// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendNone.h"
#include "Data/Blending/PCGExMetadataBlender.h"



bool FPCGExSubPointsBlendNone::PrepareForData(
	FPCGExContext* InContext,
	const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
	const TSharedPtr<PCGExData::FFacade>& InSourceFacade, const PCGExData::EIOSide InSourceSide,
	const TSet<FName>* IgnoreAttributeSet)
{
	// Skip creating blender and unnecessary stuff
	return true;
}

void FPCGExSubPointsBlendNone::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	// None
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendNone::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(None)
	return NewOperation;
}
