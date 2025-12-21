// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeGrid.h"


#include "Containers/PCGExManagedObjects.h"
#include "Details/PCGExSettingsDetails.h"


#define LOCTEXT_NAMESPACE "PCGExCreateBuilderGrid"
#define PCGEX_NAMESPACE CreateBuilderGrid

bool FPCGExShapeGridBuilder::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	if (!FPCGExShapeBuilderOperation::PrepareForSeeds(InContext, InSeedDataFacade)) { return false; }

	//StartAngle = Config.GetValueSettingStartAngle();
	//if (!StartAngle->Init(InContext, InSeedDataFacade)) { return false; }

	//EndAngle = Config.GetValueSettingEndAngle();
	//if (!EndAngle->Init(InContext, InSeedDataFacade)) { return false; }

	return true;
}

void FPCGExShapeGridBuilder::PrepareShape(const PCGExData::FConstPoint& Seed)
{
	PCGEX_MAKE_SHARED(Grid, PCGExShapes::FGrid, Seed)

	Grid->ComputeFit(BaseConfig);

	Grid->StartAngle = FMath::DegreesToRadians(StartAngle->Read(Seed.Index));
	Grid->EndAngle = FMath::DegreesToRadians(EndAngle->Read(Seed.Index));
	Grid->AngleRange = FMath::Abs(Grid->EndAngle - Grid->StartAngle);

	Grid->Radius = Grid->Fit.GetExtent().Length();

	if (Config.ResolutionMode == EPCGExResolutionMode::Distance) { Grid->NumPoints = (Grid->Radius * Grid->AngleRange) * GetResolution(Seed); }
	else { Grid->NumPoints = GetResolution(Seed); }

	ValidateShape(Grid);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Grid);
}

void FPCGExShapeGridBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bIsolated)
{
	const TSharedPtr<PCGExShapes::FGrid> Grid = StaticCastSharedPtr<PCGExShapes::FGrid>(InShape);

	const double Increment = Grid->AngleRange / Grid->NumPoints;
	FVector Target = FVector::ZeroVector;

	const FVector Extents = Grid->Fit.GetExtent();
	const FVector Center = Grid->Fit.GetCenter();

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	for (int32 i = 0; i < Grid->NumPoints; i++)
	{
		const double A = (Grid->StartAngle + Increment * 0.5) + i * Increment;

		const FVector P = Center + FVector(Extents.X * FMath::Cos(A), Extents.Y * FMath::Sin(A), 0);

		if (Config.PointsLookAt == EPCGExShapePointLookAt::None)
		{
			Target = Center + FVector(Extents.X * FMath::Cos(A + 0.001), Extents.Y * FMath::Sin(A + 0.001), 0);
		}

		OutTransforms[Scope.Start + i] = FTransform(PCGExMath::MakeLookAtTransform(P - Target, FVector::UpVector, Config.LookAtAxis).GetRotation(), P, FVector::OneVector);
	}
}

PCGEX_SHAPE_BUILDER_BOILERPLATE(Grid)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
