// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineFilter.h"


#define LOCTEXT_NAMESPACE "PCGExSplineFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineFilterDefinition

bool UPCGExSplineFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
	if (!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData) { continue; }

			switch (Config.SampleInputs)
			{
			default:
			case EPCGExSplineSamplingIncludeMode::All:
				Splines.Add(&SplineData->SplineStruct);
				break;
			case EPCGExSplineSamplingIncludeMode::ClosedLoopOnly:
				if (SplineData->SplineStruct.bClosedLoop) { Splines.Add(&SplineData->SplineStruct); }
				break;
			case EPCGExSplineSamplingIncludeMode::OpenSplineOnly:
				if (!SplineData->SplineStruct.bClosedLoop) { Splines.Add(&SplineData->SplineStruct); }
				break;
			}
		}
	}

	if (Splines.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (either no input or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExSplineFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TSplineFilter>(this);
}

void UPCGExSplineFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

void UPCGExSplineFilterFactory::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	Super::RegisterConsumableAttributes(InContext);
	//TODO : Implement Consumable
}

bool PCGExPointsFilter::TSplineFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	ToleranceSquared = FMath::Square(TypedFilterFactory->Config.Tolerance);

	return true;
}

bool PCGExPointsFilter::TSplineFilter::Test(const int32 PointIndex) const
{
	const FTransform PtTransform = PointDataFacade->Source->GetInPoint(PointIndex).Transform;
	const FVector PtLoc = PtTransform.GetLocation();

	const double Tolerance = TypedFilterFactory->Config.Tolerance;
	double ClosestDist = MAX_dbl;
	bool bIsOn = false;
	bool bIsInsideAny = false;
	bool bIsOutsideAny = false;

	if (TypedFilterFactory->Config.Favor == EPCGExInsideOutFavor::Closest)
	{
		for (const FPCGSplineStruct* Spline : Splines)
		{
			const FTransform SampledTransform = Spline->GetTransformAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(PtLoc), ESplineCoordinateSpace::World, TypedFilterFactory->Config.bSplineScaleTolerance);
			double Dist = FVector::DistSquared(SampledTransform.GetLocation(), PtLoc);

			if (Dist > ClosestDist) { continue; }

			bIsOn = Dist < SampledTransform.GetScale3D().Length() * ToleranceSquared;

			const FVector Dir = (SampledTransform.GetLocation() - PtLoc).GetSafeNormal();
			const double Dot = FVector::DotProduct(SampledTransform.GetRotation().GetRightVector(), Dir);

			if (Dot > 0)
			{
				bIsInsideAny = true;
				bIsOutsideAny = false;
			}
			else
			{
				bIsOutsideAny = true;
				bIsInsideAny = false;
			}
		}
	}
	else
	{
		for (const FPCGSplineStruct* Spline : Splines)
		{
			const FTransform SampledTransform = Spline->GetTransformAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(PtLoc), ESplineCoordinateSpace::World, TypedFilterFactory->Config.bSplineScaleTolerance);

			double Dist = FVector::DistSquared(SampledTransform.GetLocation(), PtLoc);

			if (!bIsOn) { bIsOn = Dist < SampledTransform.GetScale3D().Length() * ToleranceSquared; }

			const FVector Dir = (SampledTransform.GetLocation() - PtLoc).GetSafeNormal();
			const double Dot = FVector::DotProduct(SampledTransform.GetRotation().GetRightVector(), Dir);

			if (Dot > 0) { bIsInsideAny = true; }
			else { bIsOutsideAny = true; }
		}
	}

	bool bPass = false;
	switch (TypedFilterFactory->Config.CheckType)
	{
	case EPCGExSplineCheckType::IsInside:
		bPass = bIsInsideAny && !bIsOn;
		break;
	case EPCGExSplineCheckType::IsInsideOrOn:
		bPass = bIsInsideAny || bIsOn;
		break;
	case EPCGExSplineCheckType::IsOutside:
		bPass = bIsOutsideAny && !bIsOn;
		break;
	case EPCGExSplineCheckType::IsOutsideOrOn:
		bPass = bIsOutsideAny || bIsOn;
		break;
	case EPCGExSplineCheckType::IsOn:
		bPass = bIsOn;
		break;
	}

	return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
}

TArray<FPCGPinProperties> UPCGExSplineFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Spline)

#if WITH_EDITOR
FString UPCGExSplineFilterProviderSettings::GetDisplayName() const
{
	switch (Config.CheckType)
	{
	default:
	case EPCGExSplineCheckType::IsInside: return TEXT("Is Inside");
	case EPCGExSplineCheckType::IsInsideOrOn: return TEXT("Is Inside or On");
	case EPCGExSplineCheckType::IsOutside: return TEXT("Is Outside");
	case EPCGExSplineCheckType::IsOutsideOrOn: return TEXT("Is Outside or On");
	case EPCGExSplineCheckType::IsOn: return TEXT("Is On");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
