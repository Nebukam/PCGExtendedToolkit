// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExTangentsOperation.h"

#include "..\..\..\Public\Data\PCGExPointIO.h"

void UPCGExTangentsOperation::PrepareForData(FPCGExPointIO& InPath)
{
	UPCGMetadata* Metadata = InPath.GetOut()->Metadata;
	ArriveAttribute = Metadata->FindOrCreateAttribute(ArriveName, FVector::ZeroVector);
	LeaveAttribute = Metadata->FindOrCreateAttribute(LeaveName, FVector::ZeroVector);
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
