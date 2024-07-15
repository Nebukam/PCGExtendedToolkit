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
	const PCGExData::FPointRef StartPoint = PCGExData::FPointRef(Points[0], 0);
	const PCGExData::FPointRef EndPoint = PCGExData::FPointRef(Points.Last(), Points.Num() - 1);
	ProcessSubPoints(StartPoint, EndPoint, Path, PCGExMath::FPathMetricsSquared());
}

void UPCGExSubPointsOperation::ProcessSubPoints(const PCGExData::FPointRef& Start, const PCGExData::FPointRef& End, const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const
{
}

void UPCGExSubPointsOperation::ProcessSubPoints(const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics, const int32 Offset) const
{
	const FPCGPoint& Start = SubPoints[0];
	const int32 LastIndex = SubPoints.Num() - 1;
	const FPCGPoint& End = SubPoints[LastIndex];
	ProcessSubPoints(PCGExData::FPointRef(Start, 0), PCGExData::FPointRef(End, LastIndex), SubPoints, Metrics);
}
