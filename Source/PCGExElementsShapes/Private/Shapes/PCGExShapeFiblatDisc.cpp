// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeFiblatDisc.h"

#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExMTCommon.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderFiblatDisc"
#define PCGEX_NAMESPACE CreateBuilderFiblatDisc

void FPCGExShapeFiblatDiscBuilder::PrepareShape(const PCGExData::FConstPoint& Seed)
{
	PCGEX_MAKE_SHARED(Fiblat, PCGExShapes::FFiblat, Seed)

	Fiblat->ComputeFit(BaseConfig);
	PrepareLattice(Fiblat, Seed);

	// Ellipse with the fit's half-sizes as semi-axes
	const FVector Extents = Fiblat->Fit.GetExtent();
	const double Area = PI * Extents.X * Extents.Y;

	if (Config.ResolutionMode == EPCGExResolutionMode::Distance)
	{
		Fiblat->NumPoints = CountFromSpacing(Area, GetResolution(Seed));
	}
	else
	{
		Fiblat->NumPoints = static_cast<int32>(GetResolution(Seed));
	}

	ComputeExtents(Fiblat, Area);

	ValidateShape(Fiblat);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Fiblat);
}

void FPCGExShapeFiblatDiscBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bOwnsData)
{
	const TSharedPtr<PCGExShapes::FFiblat> Fiblat = StaticCastSharedPtr<PCGExShapes::FFiblat>(InShape);

	const FVector Extents = Fiblat->Fit.GetExtent();
	const FVector Center = Fiblat->Fit.GetCenter();
	const int32 Count = Fiblat->NumPoints;
	const bool bLookAtSeed = Config.PointsLookAt == EPCGExShapePointLookAt::Seed;

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	PCGExMT::ParallelOrSequential(
		Count,
		[&](const int32 i)
		{
			const FVector P = Center + Fiblat->ComputeDiscPoint(i, Count) * Extents;
			const FQuat Rotation = bLookAtSeed ? LookAtRotation(P - Center, Config.LookAtAxis) : FQuat::Identity;
			OutTransforms[Scope.Start + i] = FTransform(Rotation, P, FVector::OneVector);
		});
}


PCGEX_SHAPE_BUILDER_BOILERPLATE(FiblatDisc)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
