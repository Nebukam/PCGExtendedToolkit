// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExTangentsOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExTangentsOperation::PrepareForData(const UPCGExPointIO* InPath)
{
	ArriveAttribute = InPath->Out->Metadata->FindOrCreateAttribute(ArriveName, FVector::ZeroVector);
	LeaveAttribute = InPath->Out->Metadata->FindOrCreateAttribute(LeaveName, FVector::ZeroVector);
}

void UPCGExTangentsOperation::ProcessFirstPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& NextPoint) const
{
	WriteTangents(Point.MetadataEntry, FVector::ZeroVector, FVector::ZeroVector);
}

void UPCGExTangentsOperation::ProcessLastPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint) const
{
	WriteTangents(Point.MetadataEntry, FVector::ZeroVector, FVector::ZeroVector);
}

void UPCGExTangentsOperation::ProcessPoint(const int32 Index, const FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	WriteTangents(Point.MetadataEntry, FVector::ZeroVector, FVector::ZeroVector);
}

void UPCGExTangentsOperation::WriteTangents(const PCGMetadataEntryKey Key, const FVector& Arrive, const FVector& Leave) const
{
	ArriveAttribute->SetValue(Key, Arrive);
	LeaveAttribute->SetValue(Key, Leave);
}
