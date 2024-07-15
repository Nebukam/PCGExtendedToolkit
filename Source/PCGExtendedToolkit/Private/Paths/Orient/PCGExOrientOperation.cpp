// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientOperation.h"

#include "Data/PCGExPointIO.h"

void UPCGExOrientOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExOrientOperation* TypedOther = Cast<UPCGExOrientOperation>(Other);
	if (TypedOther)
	{
		OrientAxis = TypedOther->OrientAxis;
		UpAxis = TypedOther->UpAxis;
	}
}

void UPCGExOrientOperation::PrepareForData(PCGExData::FFacade* InDataFacade)
{
}

FTransform UPCGExOrientOperation::ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const
{
	return Point.Point->Transform;
}
