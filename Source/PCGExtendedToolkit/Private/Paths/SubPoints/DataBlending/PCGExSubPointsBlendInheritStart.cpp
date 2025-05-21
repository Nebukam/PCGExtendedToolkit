// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInheritStart.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Sampling/PCGExGetTextureData.h"


void FPCGExSubPointsBlendInheritStart::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	const TArrayView<FPCGPoint>& SubPoints, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	for (int i = 0; i < SubPoints.Num(); i++)
	{
		FVector Location = SubPoints[i].Transform.GetLocation();
		MetadataBlender->Blend(From.Index, To.Index, StartIndex < 0 ? From.Index : StartIndex, 0);
		SubPoints[i].Transform.SetLocation(Location);
	}
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInheritStart::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(InheritStart)
	return NewOperation;
}
