// Fill out your copyright notice in the Description page of Project Settings.


#include "Splines/Tangents/PCGExCustomTangents.h"

#include "Data/PCGExPointIO.h"

void UPCGExCustomTangents::PrepareForData(const UPCGExPointIO* InPath)
{
	Super::PrepareForData(InPath);
	Arrive.PrepareForData(InPath->Out);
	Leave.PrepareForData(InPath->Out);
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
