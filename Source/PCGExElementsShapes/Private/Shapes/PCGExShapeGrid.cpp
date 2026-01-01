// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeGrid.h"

#include "PropertyAccess.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"
#include "Details/PCGExSettingsDetails.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderGrid"
#define PCGEX_NAMESPACE CreateBuilderGrid

bool FPCGExShapeGridBuilder::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	if (!FPCGExShapeBuilderOperation::PrepareForSeeds(InContext, InSeedDataFacade)) { return false; }
	return true;
}

void FPCGExShapeGridBuilder::PrepareShape(const PCGExData::FConstPoint& Seed)
{
	PCGEX_MAKE_SHARED(Grid, PCGExShapes::FGrid, Seed)

	Grid->ComputeFit(BaseConfig);

	const FVector Res = GetResolutionVector(Seed);
	const FVector Size = Grid->Fit.GetSize();

	if (Config.ResolutionMode == EPCGExResolutionMode::Fixed)
	{
		Grid->Count.X = FMath::Max(1, FMath::FloorToInt32(Res.X));
		Grid->Count.Y = FMath::Max(1, FMath::FloorToInt32(Res.Y));
		Grid->Count.Z = FMath::Max(1, FMath::FloorToInt32(Res.Z));

		Grid->Extents.X = (Size.X / Grid->Count.X) * 0.5;
		Grid->Extents.Y = (Size.Y / Grid->Count.Y) * 0.5;
		Grid->Extents.Z = (Size.Z / Grid->Count.Z) * 0.5;
	}
	else
	{
		Grid->Count.X = FMath::Max(1, FMath::FloorToInt32(Size.X / Res.X));
		Grid->Count.Y = FMath::Max(1, FMath::FloorToInt32(Size.Y / Res.Y));
		Grid->Count.Z = FMath::Max(1, FMath::FloorToInt32(Size.Z / Res.Z));

		if (Config.bAdjustFit)
		{
			Grid->Extents.X = (Size.X / Grid->Count.X) * 0.5;
			Grid->Extents.Y = (Size.Y / Grid->Count.Y) * 0.5;
			Grid->Extents.Z = (Size.Z / Grid->Count.Z) * 0.5;
		}
		else
		{
			Grid->Extents = Res * 0.5;
		}
	}

	Grid->Offset.X = Grid->Extents.X + (Size.X - (Grid->Count.X * (Grid->Extents.X * 2))) * 0.5;
	Grid->Offset.Y = Grid->Extents.Y + (Size.Y - (Grid->Count.Y * (Grid->Extents.Y * 2))) * 0.5;
	Grid->Offset.Z = Grid->Extents.Z + (Size.Z - (Grid->Count.Z * (Grid->Extents.Z * 2))) * 0.5;

	Grid->bClosedLoop = false;

	Grid->NumPoints = Grid->Count.X * Grid->Count.Y * Grid->Count.Z;

	ValidateShape(Grid);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Grid);
}

void FPCGExShapeGridBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bOwnsData)
{
	const TSharedPtr<PCGExShapes::FGrid> Grid = StaticCastSharedPtr<PCGExShapes::FGrid>(InShape);

	const FVector Center = Grid->Fit.GetCenter();
	const FVector Corner = Center - Grid->Fit.GetExtent();

	const double XStep = Grid->Extents.X * 2;
	const double YStep = Grid->Extents.Y * 2;
	const double ZStep = Grid->Extents.Z * 2;

	const FVector MaxBounds = FVector(XStep, YStep, ZStep) * 0.5;
	const FVector MinBounds = -MaxBounds;

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);
	TPCGValueRange<FVector> OutBoundsMin = Scope.Data->GetBoundsMinValueRange(false);
	TPCGValueRange<FVector> OutBoundsMax = Scope.Data->GetBoundsMaxValueRange(false);

	PCGEX_PARALLEL_FOR(
		Scope.Count,
		
		const int32 WriteIndex = Scope.Start + i;

		OutBoundsMin[WriteIndex] = MinBounds;
		OutBoundsMax[WriteIndex] = MaxBounds;

		const int32 X = i % Grid->Count.X;
		const int32 Y = (i / Grid->Count.X) % Grid->Count.Y;
		const int32 Z = i / (Grid->Count.X * Grid->Count.Y);

		FVector Target;
		FVector Point = FVector(
				Corner.X + (X * XStep),
				Corner.Y + (Y * YStep),
				Corner.Z + (Z * ZStep))
			+ Grid->Offset;

		if (Config.PointsLookAt == EPCGExShapePointLookAt::None)
		{
			OutTransforms[WriteIndex] = FTransform(FRotator::ZeroRotator, Point, FVector::OneVector);
		}
		else
		{
			switch (Config.PointsLookAt)
			{
			case EPCGExShapePointLookAt::Seed:
				Target = Center;
				break;
			case EPCGExShapePointLookAt::None:
				Target = Point + FVector(0, 0, 1);
				break;
			default:
				Target = Point;
				break;
			}

			OutTransforms[WriteIndex] = FTransform(
				PCGExMath::MakeLookAtTransform(Point - Target, FVector::UpVector, Config.LookAtAxis).GetRotation(),
				Point,
				FVector::OneVector
			);
		}
	)

	if (bOwnsData && Grid->bClosedLoop)
	{
		PCGExPaths::Helpers::SetClosedLoop(InDataFacade->GetOut(), true);
	}
}

PCGEX_SHAPE_BUILDER_BOILERPLATE(Grid)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
