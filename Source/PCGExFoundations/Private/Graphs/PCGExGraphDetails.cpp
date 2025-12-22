// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graphs/PCGExGraphDetails.h"


#include "PCGExCoreSettingsCache.h"
#include "Data/PCGExPointElements.h"

void FPCGExBasicEdgeSolidificationDetails::Mutate(PCGExData::FMutablePoint& InEdgePoint, const PCGExData::FConstPoint& InStart, const PCGExData::FConstPoint& InEnd, const double InLerp) const
{
	const FVector A = InStart.GetLocation();
	const FVector B = InEnd.GetLocation();

	InEdgePoint.SetLocation(FMath::Lerp(A, B, InLerp));
	if (SolidificationAxis == EPCGExMinimalAxis::None) { return; }

	const FVector EdgeDirection = (A - B).GetSafeNormal();

	const double EdgeLength = FVector::Dist(A, B);
	const double StartRadius = InStart.GetScaledExtents().Size();
	const double EndRadius = InEnd.GetScaledExtents().Size();

	double Rad = 0;

	switch (RadiusType)
	{
	case EPCGExBasicEdgeRadius::Average: Rad = (StartRadius + EndRadius) * 0.5 * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Lerp: Rad = FMath::Lerp(StartRadius, EndRadius, InLerp) * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Min: Rad = FMath::Min(StartRadius, EndRadius) * RadiusScale;
		break;
	case EPCGExBasicEdgeRadius::Max: Rad = FMath::Max(StartRadius, EndRadius) * RadiusScale;
		break;
	default: case EPCGExBasicEdgeRadius::Fixed: Rad = RadiusConstant;
		break;
	}

	FRotator EdgeRot;
	FVector BoundsMin = FVector(-Rad);
	FVector BoundsMax = FVector(Rad);

	const FVector PtScale = InEdgePoint.GetScale3D();
	const FVector InvScale = FVector::One() / PtScale;

	const double LerpInv = 1 - InLerp;
	bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
	bProcessAxis = SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
	if (bProcessAxis){\
		if (SolidificationAxis == EPCGExMinimalAxis::_AXIS){ BoundsMin._AXIS = (-EdgeLength * LerpInv) * InvScale._AXIS; BoundsMax._AXIS = (EdgeLength * InLerp) * InvScale._AXIS; } \
		else{ BoundsMin._AXIS = (-Rad) * InvScale._AXIS; BoundsMax._AXIS = (Rad) * InvScale._AXIS; }}

	PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

	switch (SolidificationAxis)
	{
	default: case EPCGExMinimalAxis::X: EdgeRot = FRotationMatrix::MakeFromX(EdgeDirection).Rotator();
		break;
	case EPCGExMinimalAxis::Y: EdgeRot = FRotationMatrix::MakeFromY(EdgeDirection).Rotator();
		break;
	case EPCGExMinimalAxis::Z: EdgeRot = FRotationMatrix::MakeFromZ(EdgeDirection).Rotator();
		break;
	}

	InEdgePoint.SetTransform(FTransform(EdgeRot, FMath::Lerp(B, A, LerpInv), InEdgePoint.GetScale3D()));

	InEdgePoint.SetBoundsMin(BoundsMin);
	InEdgePoint.SetBoundsMax(BoundsMax);
}

FPCGExGraphBuilderDetails::FPCGExGraphBuilderDetails(const EPCGExMinimalAxis InDefaultSolidificationAxis)
{
	BasicEdgeSolidification.SolidificationAxis = InDefaultSolidificationAxis;
}

bool FPCGExGraphBuilderDetails::WantsClusters() const
{
	PCGEX_GET_OPTION_STATE(BuildAndCacheClusters, bDefaultBuildAndCacheClusters)
}

bool FPCGExGraphBuilderDetails::IsValid(const int32 NumVtx, const int32 NumEdges) const
{
	if (bRemoveBigClusters)
	{
		if (NumEdges > MaxEdgeCount || NumVtx > MaxVtxCount) { return false; }
	}

	if (bRemoveSmallClusters)
	{
		if (NumEdges < MinEdgeCount || NumVtx < MinVtxCount) { return false; }
	}

	return true;
}
