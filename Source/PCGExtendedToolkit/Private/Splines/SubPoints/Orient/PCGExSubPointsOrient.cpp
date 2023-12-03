// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/Orient/PCGExSubPointsOrient.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsOrient::PrepareForData(const UPCGExPointIO* InData, PCGEx::FAttributeMap* InAttributeMap)
{
	Super::PrepareForData(InData, InAttributeMap);
}

void UPCGExSubPointsOrient::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
}
