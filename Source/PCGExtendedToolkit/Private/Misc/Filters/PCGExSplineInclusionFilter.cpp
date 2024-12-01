// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineInclusionFilter.h"

#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExSplineInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineInclusionFilterDefinition

bool UPCGExSplineInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
	if (!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData) { continue; }

			const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			Splines.Add(&SplineData->SplineStruct);
		}
	}

	if (Splines.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (either no input or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExSplineInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TSplineInclusionFilter>(this);
}

void UPCGExSplineInclusionFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

void UPCGExSplineInclusionFilterFactory::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	Super::RegisterConsumableAttributes(InContext);
	//TODO : Implement Consumable
}

namespace PCGExPointsFilter
{
	bool TSplineInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		ToleranceSquared = FMath::Square(TypedFilterFactory->Config.Tolerance);

		switch (TypedFilterFactory->Config.CheckType)
		{
		case EPCGExSplineCheckType::IsInside:
			GoodFlags = Inside;
			BadFlags = On;
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsInsideOrOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Inside | On);
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsInsideAndOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Inside | On);
			GoodMatch = All;
			break;
		case EPCGExSplineCheckType::IsOutside:
			GoodFlags = Outside;
			BadFlags = On;
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideOrOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Outside | On);
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsOutsideAndOn:
			GoodFlags = static_cast<ESplineCheckFlags>(Outside | On);
			GoodMatch = All;
			break;
		case EPCGExSplineCheckType::IsOn:
			GoodFlags = On;
			GoodMatch = Any;
			break;
		case EPCGExSplineCheckType::IsNotOn:
			BadFlags = On;
			GoodMatch = Skip;
			break;
		}

		return true;
	}

	bool TSplineInclusionFilter::Test(const int32 PointIndex) const
	{
		uint8 State = None;

		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (const FPCGSplineStruct* Spline : Splines)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				const double D = FVector::DistSquared(Pos, TLoc);

				if (D > ClosestDist) { continue; }
				ClosestDist = D;

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				else { State &= ~On; }

				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) > TypedFilterFactory->Config.CurvatureThreshold)
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
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(T.GetLocation(), Pos) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) > TypedFilterFactory->Config.CurvatureThreshold) { State |= Inside; }
				else { State |= Outside; }
			}
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? (State & GoodFlags) != 0 : (State & GoodFlags) == GoodFlags; } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}
}

TArray<FPCGPinProperties> UPCGExSplineInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(SplineInclusion)

#if WITH_EDITOR
FString UPCGExSplineInclusionFilterProviderSettings::GetDisplayName() const
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
