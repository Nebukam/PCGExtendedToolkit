// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExCustomTangents.h"

#include "Data/PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(PCGExData::FPointIO& InPath)
{
	Super::PrepareForData(InPath);
	Arrive.PrepareForData(InPath.GetOut());
	Leave.PrepareForData(InPath.GetOut());
}

void UPCGExCustomTangents::ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const FPCGPoint& Point = *MainPoint.Point;
	OutArrive = Arrive.GetTangent(Point);
	OutLeave = bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point);
}

void UPCGExCustomTangents::ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const FPCGPoint& Point = *MainPoint.Point;
	OutArrive = Arrive.GetTangent(Point);
	OutLeave = bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point);
}

void UPCGExCustomTangents::ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const FPCGPoint& Point = *MainPoint.Point;
	OutArrive = Arrive.GetTangent(Point);
	OutLeave = bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point);
}
