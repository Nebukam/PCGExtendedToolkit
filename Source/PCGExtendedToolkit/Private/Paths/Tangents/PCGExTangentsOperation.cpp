// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExTangentsOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExTangentsOperation::PrepareForData(PCGExData::FPointIO& InPointIO)
{
}

void UPCGExTangentsOperation::ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
}
