// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExCustomTangents.h"

#include "Data/PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(PCGExData::FFacade* InDataFacade)
{
	Super::PrepareForData(InDataFacade);
	Arrive.PrepareForData(InDataFacade);
	Leave.PrepareForData(InDataFacade);
}

void UPCGExCustomTangents::ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
	OutArrive = Arrive.GetTangent(0);
	OutLeave = bMirror ? Arrive.GetTangent(0) : Leave.GetTangent(0);
}

void UPCGExCustomTangents::ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = InPoints.Num()-1;
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const
{
	OutArrive = Arrive.GetTangent(Index);
	OutLeave = bMirror ? Arrive.GetTangent(Index) : Leave.GetTangent(Index);
}

void UPCGExCustomTangents::Cleanup()
{
	Arrive.Cleanup();
	Leave.Cleanup();
	Super::Cleanup();
}
