// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/PCGExSubPointsOperation.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExSubPointsOperation* TypedOther = Cast<UPCGExSubPointsOperation>(Other);
	if (TypedOther)
	{
		bClosedPath = TypedOther->bClosedPath;
	}
}

void UPCGExSubPointsOperation::PrepareForData(PCGExData::FFacade* InPrimaryFacade)
{
}

void UPCGExSubPointsOperation::ProcessPoints(UPCGPointData* InData) const
{
	TArray<FPCGPoint>& Points = InData->GetMutablePoints();
	TArrayView<FPCGPoint> Path = MakeArrayView(Points.GetData(), Points.Num());
	const PCGEx::FPointRef StartPoint = PCGEx::FPointRef(Points[0], 0);
	const PCGEx::FPointRef EndPoint = PCGEx::FPointRef(Points.Last(), Points.Num() - 1);
	ProcessSubPoints(StartPoint, EndPoint, Path, PCGExMath::FPathMetricsSquared());
}

void UPCGExSubPointsOperation::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const
{
}

void UPCGExSubPointsOperation::ProcessSubPoints(const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const int32 Offset) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	ProcessSubPoints(PCGEx::FPointRef(Start, 0), PCGEx::FPointRef(End, LastIndex), SubPoints, Metrics);
}
