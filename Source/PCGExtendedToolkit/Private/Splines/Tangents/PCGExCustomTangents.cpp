// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExCustomTangents.h"

#include "..\..\..\Public\Data\PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(PCGExData::FPointIO& InPath)
{
	Super::PrepareForData(InPath);
	Arrive.PrepareForData(InPath.GetOut());
	Leave.PrepareForData(InPath.GetOut());
}

void UPCGExCustomTangents::ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const
{
	WriteTangents(Point.MetadataEntry, Arrive.GetTangent(Point), bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point));
}

void UPCGExCustomTangents::ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const
{
	WriteTangents(Point.MetadataEntry, Arrive.GetTangent(Point), bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point));
}

void UPCGExCustomTangents::ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	WriteTangents(Point.MetadataEntry, Arrive.GetTangent(Point), bMirror ? Arrive.GetTangent(Point) : Leave.GetTangent(Point));
}
