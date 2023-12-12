// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExCustomTangents.h"

#include "Data/PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(PCGExData::FPointIO& InPath)
{
	Super::PrepareForData(InPath);
	Arrive.PrepareForData(InPath);
	Leave.PrepareForData(InPath);
}

void UPCGExCustomTangents::ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = MainPoint.Index;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::Cleanup()
{
	PCGEX_CLEANUP(Arrive)
	PCGEX_CLEANUP(Leave)
	Super::Cleanup();
}
