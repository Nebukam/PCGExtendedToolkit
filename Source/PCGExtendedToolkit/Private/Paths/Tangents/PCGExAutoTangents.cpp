// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExAutoTangents.h"

#include "PCGExMath.h"
#include "Data/PCGExPointIO.h"

void UPCGExAutoTangents::ProcessFirstPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromStartOnly(
		NextPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessLastPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromEndOnly(
		PreviousPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessPoint(const PCGExData::FPointRef& MainPoint, const PCGExData::FPointRef& PreviousPoint, const PCGExData::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex(
		PreviousPoint.Point->Transform.GetLocation(),
		NextPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(Scale, FName(TEXT("Tangents/Scale")), EPCGMetadataTypes::Double);
}
