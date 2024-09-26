// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendNone.h"

#include "Data/Blending/PCGExMetadataBlender.h"


void UPCGExSubPointsBlendNone::BlendSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender, const int32 StartIndex) const
{
}

TSharedPtr<PCGExDataBlending::FMetadataBlender> UPCGExSubPointsBlendNone::CreateBlender(
	const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade,
	const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade,
	const PCGExData::ESource SecondarySource, const TSet<FName>* IgnoreAttributeSet)
{
	TSharedPtr<PCGExDataBlending::FMetadataBlender> NewBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&BlendingDetails);
	NewBlender->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource);
	return NewBlender;
}
