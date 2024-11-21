// Copyright Timothé Lapetite 2024
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

			CreateSpline(PathData, bIsClosedLoop);
		}
	}

	if (Splines.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (either no input or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPathInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TPathInclusionFilter>(this);
}

void UPCGExPathInclusionFilterFactory::CreateSpline(const UPCGPointData* InData, const bool bClosedLoop)
{
	const TArray<FPCGPoint>& InPoints = InData->GetPoints();
	if (InPoints.Num() < 2) { return; }

	const int32 NumPoints = InPoints.Num();

	TArray<FSplinePoint> SplinePoints;
	PCGEx::InitArray(SplinePoints, NumPoints);

	ESplinePointType::Type PointType = ESplinePointType::Linear;

	bool bComputeTangents = false;
	switch (Config.PointType)
	{
	case EPCGExSplinePointTypeRedux::Linear:
		PointType = ESplinePointType::CurveCustomTangent;
		bComputeTangents = true;
		break;
	case EPCGExSplinePointTypeRedux::Curve:
		PointType = ESplinePointType::Curve;
		break;
	case EPCGExSplinePointTypeRedux::Constant:
		PointType = ESplinePointType::Constant;
		break;
	case EPCGExSplinePointTypeRedux::CurveClamped:
		PointType = ESplinePointType::CurveClamped;
		break;
	}

	if (bComputeTangents)
	{
		const int32 MaxIndex = NumPoints - 1;

		for (int i = 0; i < NumPoints; i++)
		{
			const FTransform TR = InPoints[i].Transform;
			const FVector PtLoc = InPoints[i].Transform.GetLocation();

			const FVector PrevDir = (InPoints[i == 0 ? bClosedLoop ? MaxIndex : 0 : i - 1].Transform.GetLocation() - PtLoc) * -1;
			const FVector NextDir = InPoints[i == MaxIndex ? bClosedLoop ? 0 : i : i + 1].Transform.GetLocation() - PtLoc;
			const FVector Tangent = FMath::Lerp(PrevDir, NextDir, 0.5).GetSafeNormal() * 0.01;

			SplinePoints[i] = FSplinePoint(
				static_cast<float>(i),
				TR.GetLocation(),
				Tangent,
				Tangent,
				TR.GetRotation().Rotator(),
				TR.GetScale3D(),
				PointType);
		}
	}
	else
	{
		for (int i = 0; i < NumPoints; i++)
		{
			const FTransform TR = InPoints[i].Transform;
			SplinePoints[i] = FSplinePoint(
				static_cast<float>(i),
				TR.GetLocation(),
				FVector::ZeroVector,
				FVector::ZeroVector,
				TR.GetRotation().Rotator(),
				TR.GetScale3D(),
				PointType);
		}
	}

	TSharedPtr<FPCGSplineStruct> SplineStruct = MakeShared<FPCGSplineStruct>();
	SplineStruct->Initialize(SplinePoints, bClosedLoop, FTransform::Identity);
	Splines.Add(SplineStruct);
}

void UPCGExPathInclusionFilterFactory::BeginDestroy()
{
	Splines.Empty();
	Super::BeginDestroy();
}

void UPCGExPathInclusionFilterFactory::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	Super::RegisterConsumableAttributes(InContext);
	//TODO : Implement Consumable
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
				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				else { State &= ~On; }

				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (T.GetLocation() - Pos).GetSafeNormal()) > TypedFilterFactory->Config.CurvatureThreshold)
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
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(T.GetLocation(), Pos) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { State |= On; }
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (T.GetLocation() - Pos).GetSafeNormal()) > TypedFilterFactory->Config.CurvatureThreshold) { State |= Inside; }
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
