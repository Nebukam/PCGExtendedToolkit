// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/Builders/PCGExShapeCircle.h"


#define LOCTEXT_NAMESPACE "PCGExCreateBuilderCircle"
#define PCGEX_NAMESPACE CreateBuilderCircle

bool UPCGExShapeCircleBuilder::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	if (!Super::PrepareForSeeds(InContext, InSeedDataFacade)) { return false; }

	if (Config.StartAngleInput == EPCGExInputValueType::Attribute)
	{
		StartAngleGetter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!StartAngleGetter->Prepare(Config.StartAngleAttribute, InSeedDataFacade->Source)) { return false; }
	}

	if (Config.EndAngleInput == EPCGExInputValueType::Attribute)
	{
		EndAngleGetter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!EndAngleGetter->Prepare(Config.EndAngleAttribute, InSeedDataFacade->Source)) { return false; }
	}

	return true;
}

void UPCGExShapeCircleBuilder::PrepareShape(const PCGExData::FPointRef& Seed)
{
	TSharedPtr<PCGExShapes::FCircle> Circle = MakeShared<PCGExShapes::FCircle>(Seed);
	
	Circle->ComputeFit(Config);
	
	Circle->StartAngle = FMath::DegreesToRadians(StartAngleGetter ? StartAngleGetter->SoftGet(Seed, Config.StartAngleConstant) : Config.StartAngleConstant);
	Circle->EndAngle = FMath::DegreesToRadians(EndAngleGetter ? EndAngleGetter->SoftGet(Seed, Config.EndAngleConstant) : Config.EndAngleConstant);
	Circle->AngleRange = FMath::Abs(Circle->EndAngle - Circle->StartAngle);

	Circle->Radius = Circle->Fit.GetExtent().X;

	const double Resolution = GetResolution(Seed);

	if (Config.ResolutionMode == EPCGExResolutionMode::Distance) { Circle->NumPoints = (Circle->Radius * Circle->AngleRange) * Resolution; }
	else { Circle->NumPoints = Resolution; }

	ValidateShape(Circle);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Circle);
}

void UPCGExShapeCircleBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const TArrayView<FPCGPoint> PointView)
{
	const TSharedPtr<PCGExShapes::FCircle> Circle = StaticCastSharedPtr<PCGExShapes::FCircle>(InShape);

	const double Increment = Circle->AngleRange / Circle->NumPoints;
	const double StartAngle = Circle->StartAngle + Increment * 0.5;
	FVector Target = FVector::ZeroVector;

	for (int32 i = 0; i < Circle->NumPoints; i++)
	{
		const double A = StartAngle + i * Increment;

		const FVector P = FVector(Circle->Radius * FMath::Cos(A), Circle->Radius * FMath::Sin(A), 0);

		if (Config.PointsLookAt == EPCGExShapePointLookAt::None) { Target = FVector(Circle->Radius * FMath::Cos(A + 0.001), Circle->Radius * FMath::Sin(A + 0.001), 0); }

		PointView[i].Transform = FTransform(
			PCGExMath::MakeLookAtTransform(P - Target, FVector::UpVector, Config.LookAtAxis).GetRotation(),
			P, FVector::OneVector);
	}
}

PCGEX_SHAPE_BUILDER_BOILERPLATE(Circle)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
