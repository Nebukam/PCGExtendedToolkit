// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExTangentsOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExTangentsOperation::PrepareForData(PCGExData::FPointIO* InPointIO)
{
}

void UPCGExTangentsOperation::ProcessFirstPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessLastPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
}

void UPCGExTangentsOperation::ProcessPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
}
