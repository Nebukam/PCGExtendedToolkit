﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/Builders/PCGExShapeCircle.h"

#include "PCGExHelpers.h"
#include "Data/PCGExData.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderCircle"
#define PCGEX_NAMESPACE CreateBuilderCircle

PCGEX_SETTING_VALUE_IMPL(FPCGExShapeCircleConfig, EndAngle, double, EndAngleInput, EndAngleAttribute, EndAngleConstant)
PCGEX_SETTING_VALUE_IMPL(FPCGExShapeCircleConfig, StartAngle, double, StartAngleInput, StartAngleAttribute, StartAngleConstant)

bool FPCGExShapeCircleBuilder::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	if (!FPCGExShapeBuilderOperation::PrepareForSeeds(InContext, InSeedDataFacade)) { return false; }

	StartAngle = Config.GetValueSettingStartAngle();
	if (!StartAngle->Init(InSeedDataFacade)) { return false; }

	EndAngle = Config.GetValueSettingEndAngle();
	if (!EndAngle->Init(InSeedDataFacade)) { return false; }

	return true;
}

void FPCGExShapeCircleBuilder::PrepareShape(const PCGExData::FConstPoint& Seed)
{
	PCGEX_MAKE_SHARED(Circle, PCGExShapes::FCircle, Seed)

	Circle->ComputeFit(BaseConfig);

	Circle->StartAngle = FMath::DegreesToRadians(StartAngle->Read(Seed.Index));
	Circle->EndAngle = FMath::DegreesToRadians(EndAngle->Read(Seed.Index));
	Circle->AngleRange = FMath::Abs(Circle->EndAngle - Circle->StartAngle);

	Circle->Radius = Circle->Fit.GetExtent().Length();
	Circle->bClosedLoop = Config.bIsClosedLoop || FMath::IsNearlyEqual(Circle->AngleRange, TWO_PI);

	if (Config.ResolutionMode == EPCGExResolutionMode::Distance)
	{
		Circle->NumPoints = (Circle->Radius * Circle->AngleRange) / GetResolution(Seed);
	}
	else { Circle->NumPoints = GetResolution(Seed); }

	ValidateShape(Circle);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Circle);
}

void FPCGExShapeCircleBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bOwnsData)
{
	const TSharedPtr<PCGExShapes::FCircle> Circle = StaticCastSharedPtr<PCGExShapes::FCircle>(InShape);

	const double Increment = Circle->AngleRange / Circle->NumPoints;
	FVector Target = FVector::ZeroVector;

	const FVector Extents = Circle->Fit.GetExtent();
	const FVector Center = Circle->Fit.GetCenter();

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	for (int32 i = 0; i < Circle->NumPoints; i++)
	{
		const double A = (Circle->StartAngle + Increment * 0.5) + i * Increment;

		const FVector P = Center + FVector(Extents.X * FMath::Cos(A), Extents.Y * FMath::Sin(A), 0);

		if (Config.PointsLookAt == EPCGExShapePointLookAt::None)
		{
			Target = Center + FVector(Extents.X * FMath::Cos(A + 0.001), Extents.Y * FMath::Sin(A + 0.001), 0);
		}

		OutTransforms[Scope.Start + i] = FTransform(
			PCGExMath::MakeLookAtTransform(P - Target, FVector::UpVector, Config.LookAtAxis).GetRotation(),
			P, FVector::OneVector);
	}

	// Mark @Data.IsClosed if a single circle "owns" the data
	if (bOwnsData && Circle->bClosedLoop) { PCGExPaths::SetClosedLoop(InDataFacade->GetOut(), true); }
}

PCGEX_SHAPE_BUILDER_BOILERPLATE(Circle)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
