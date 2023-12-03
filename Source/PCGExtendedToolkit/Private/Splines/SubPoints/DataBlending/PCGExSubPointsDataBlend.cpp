// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlend.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsDataBlend::PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap)
{
	Super::PrepareForData(InData, InAttributeMap);
}

void UPCGExSubPointsDataBlend::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
}
