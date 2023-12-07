// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/Orient/PCGExSubPointsOrientOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExSubPointsOrientOperation::PrepareForData(UPCGExPointIO* InData)
{
	Super::PrepareForData(InData);
	//AxisGetter.Capture(Axis);
	//AxisGetter.Validate(InData->Out);
}

void UPCGExSubPointsOrientOperation::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathInfos& PathInfos) const
{
	const int32 NumPointsMinusOne = SubPoints.Num() - 1;
	Orient(SubPoints[0], StartPoint, SubPoints[1]);
	for (int i = 1; i < NumPointsMinusOne; i++) { Orient(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
	Orient(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], EndPoint);
}

void UPCGExSubPointsOrientOperation::Orient(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
}
