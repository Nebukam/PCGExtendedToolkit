// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/PCGExSubPointsProcessor.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsProcessor::PrepareForData(const UPCGExPointIO* InData)
{
}

void UPCGExSubPointsProcessor::ProcessPoints(UPCGPointData* InData) const
{
	TArray<FPCGPoint>& Points = InData->GetMutablePoints();
	TArrayView<FPCGPoint> Path = MakeArrayView(Points.GetData(), Points.Num());
	ProcessSubPoints(Points[0], Points.Last(), Path, PCGExMath::FPathInfos());
}

void UPCGExSubPointsProcessor::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
}
