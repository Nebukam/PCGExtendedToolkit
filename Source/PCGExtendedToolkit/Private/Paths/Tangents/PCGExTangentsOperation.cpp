// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExTangentsOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExTangentsOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExTangentsOperation* TypedOther = Cast<UPCGExTangentsOperation>(Other))
	{
		ArriveName = TypedOther->ArriveName;
		LeaveName = TypedOther->LeaveName;
		bClosedPath = TypedOther->bClosedPath;
	}
}

void UPCGExTangentsOperation::PrepareForData(PCGExData::FFacade* InDataFacade)
{
}

void UPCGExTangentsOperation::ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const
{
}
