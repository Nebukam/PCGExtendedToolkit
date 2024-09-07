// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/PCGExSubPointsOperation.h"

#include "Data/PCGExPointIO.h"


void UPCGExSubPointsOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsOperation* TypedOther = Cast<UPCGExSubPointsOperation>(Other))
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

void UPCGExSubPointsOperation::ProcessSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExMath::FPathMetricsSquared& Metrics,
	const int32 StartIndex) const
{
}
