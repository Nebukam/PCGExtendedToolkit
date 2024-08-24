// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Tangents/PCGExAutoTangents.h"

#include "Geometry/PCGExGeo.h"

void UPCGExAutoTangents::ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExGeo::FApex Apex = PCGExGeo::FApex::FromStartOnly(
		InPoints[0].Transform.GetLocation(),
		InPoints[0].Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const
{
	const int32 Index = InPoints.Num() - 1;
	const int32 PrevIndex = Index - 1;
	PCGExGeo::FApex Apex = PCGExGeo::FApex::FromEndOnly(
		InPoints[PrevIndex].Transform.GetLocation(),
		InPoints[Index].Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExGeo::FApex Apex = PCGExGeo::FApex(
		InPoints[PrevIndex].Transform.GetLocation(),
		InPoints[NextIndex].Transform.GetLocation(),
		InPoints[Index].Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(Scale, FName(TEXT("Tangents/Scale")), EPCGMetadataTypes::Double);
}
