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

namespace PCGExPointsFilter
{
	bool TSplineFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		ToleranceSquared = FMath::Square(TypedFilterFactory->Config.Tolerance);

		switch (TypedFilterFactory->Config.CheckType)
		{
		case EPCGExSplineCheckType::IsInside:
			CheckFlag = Inside;
			Match = Any;
			break;
		case EPCGExSplineCheckType::IsInsideOrOn:
			CheckFlag = static_cast<ESplineCheckFlags>(Inside | On);
			Match = Any;
			break;
		case EPCGExSplineCheckType::IsInsideAndOn:
			CheckFlag = static_cast<ESplineCheckFlags>(Inside | On);
			Match = All;
			break;
		case EPCGExSplineCheckType::IsOutside:
			CheckFlag = Outside;
			Match = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideOrOn:
			CheckFlag = static_cast<ESplineCheckFlags>(Outside | On);
			Match = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideAndOn:
			CheckFlag = static_cast<ESplineCheckFlags>(Outside | On);
			Match = All;
			break;
		case EPCGExSplineCheckType::IsOn:
			CheckFlag = On;
			Match = Any;
			break;
		case EPCGExSplineCheckType::IsNotOn:
			CheckFlag = On;
			Match = Not;
			break;
		}

		return true;
	}

	bool TSplineFilter::Test(const int32 PointIndex) const
	{
		uint8 State = None;

		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (const FPCGSplineStruct* Spline : Splines)
			{
				const FTransform T = Spline->GetTransformAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(Pos), ESplineCoordinateSpace::World, TypedFilterFactory->Config.bSplineScalesTolerance);
				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				else { State &= ~On; }

				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (T.GetLocation() - Pos).GetSafeNormal()) > 0)
				{
					State |= Inside;
					State &= ~Outside;
				}
				else
				{
					State |= Outside;
					State &= ~Inside;
				}
			}
		}
		else
		{
			for (const FPCGSplineStruct* Spline : Splines)
			{
				const FTransform T = Spline->GetTransformAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(Pos), ESplineCoordinateSpace::World, TypedFilterFactory->Config.bSplineScalesTolerance);
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(T.GetLocation(), Pos) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (T.GetLocation() - Pos).GetSafeNormal()) > 0) { State |= Inside; }
				else { State |= Outside; }
			}
		}

		bool bPass = true;
		if (Match == Not) { bPass = (State & CheckFlag) == 0; }
		else if (Match == Any) { bPass = (State & CheckFlag) != 0; }
		else if (Match == All) { bPass = (State & CheckFlag) == CheckFlag; }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}
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
	case EPCGExSplineCheckType::IsInsideAndOn: return TEXT("Is Outside and On");
	case EPCGExSplineCheckType::IsOutside: return TEXT("Is Outside");
	case EPCGExSplineCheckType::IsOutsideOrOn: return TEXT("Is Outside or On");
	case EPCGExSplineCheckType::IsOutsideAndOn: return TEXT("Is Outside and On");
	case EPCGExSplineCheckType::IsOn: return TEXT("Is On");
	case EPCGExSplineCheckType::IsNotOn: return TEXT("Is not On");
	}
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
