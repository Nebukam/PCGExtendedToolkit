// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathInclusionFilter.h"


#define LOCTEXT_NAMESPACE "PCGExPathInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathInclusionFilterDefinition

bool UPCGExPathInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExGraph::SourcePathsLabel);
	Config.ClosedLoop.Init();

	if (!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGPointData* PathData = Cast<UPCGPointData>(TaggedData.Data);
			if (!PathData) { continue; }

			const bool bIsClosedLoop = Config.ClosedLoop.IsClosedLoop(TaggedData);
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			if (TSharedPtr<FPCGSplineStruct> SplineStruct = PCGExPaths::MakeSplineFromPoints(PathData, Config.PointType, bIsClosedLoop))
			{
				Splines.Add(SplineStruct);
			}
		}
	}

	if (Splines.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPathInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TPathInclusionFilter>(this);
}

void UPCGExPathInclusionFilterFactory::BeginDestroy()
{
	Splines.Empty();
	Super::BeginDestroy();
}

namespace PCGExPointsFilter
{
	bool TPathInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
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

	bool TPathInclusionFilter::Test(const int32 PointIndex) const
	{
		uint8 State = None;

		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (const TSharedPtr<const FPCGSplineStruct>& Spline : Splines)
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
			for (const TSharedPtr<const FPCGSplineStruct>& Spline : Splines)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) > TypedFilterFactory->Config.CurvatureThreshold) { State |= Inside; }
				else { State |= Outside; }
			}
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? (State & GoodFlags) != 0 : (State & GoodFlags) == GoodFlags; } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}
}

TArray<FPCGPinProperties> UPCGExPathInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(PathInclusion)

#if WITH_EDITOR
FString UPCGExPathInclusionFilterProviderSettings::GetDisplayName() const
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
