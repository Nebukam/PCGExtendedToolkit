// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendNone.h"

#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExSubPointsBlendNone::BlendSubPoints(
	const PCGExData::FPointRef& StartPoint,
	const PCGExData::FPointRef& EndPoint,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender) const
{
	
}

PCGExDataBlending::FMetadataBlender* UPCGExSubPointsBlendNone::CreateBlender(
	PCGExData::FFacade* InPrimaryFacade,
	PCGExData::FFacade* InSecondaryFacade,
	const PCGExData::ESource SecondarySource)
{
	PCGExDataBlending::FMetadataBlender* NewBlender = new PCGExDataBlending::FMetadataBlender(&BlendingDetails);
	NewBlender->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);
	return NewBlender;
}
