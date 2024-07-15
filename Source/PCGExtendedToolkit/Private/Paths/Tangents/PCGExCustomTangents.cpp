// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExCustomTangents.h"

#include "Data/PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(PCGExData::FPointIO* InPointIO)
{
	Super::PrepareForData(InPointIO);
	Arrive.PrepareForData(InPointIO);
	Leave.PrepareForData(InPointIO);
}

void UPCGExCustomTangents::ProcessFirstPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::ProcessLastPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::ProcessPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::Cleanup()
{
	Arrive.Cleanup();
	Leave.Cleanup();
	Super::Cleanup();
}
