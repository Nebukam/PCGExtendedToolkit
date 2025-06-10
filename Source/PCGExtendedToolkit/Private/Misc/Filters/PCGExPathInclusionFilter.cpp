// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathInclusionFilter.h"


#define LOCTEXT_NAMESPACE "PCGExPathInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathInclusionFilterDefinition

bool UPCGExPathInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	Config.ClosedLoop.Init();

	return true;
}

bool UPCGExPathInclusionFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExPathInclusionFilterFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	Splines = MakeShared<TArray<TSharedPtr<FPCGSplineStruct>>>();

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::SourcePathsLabel);
		!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data);
			if (!PathData) { continue; }

			const bool bIsClosedLoop = Config.ClosedLoop.IsClosedLoop(TaggedData);
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			if (TSharedPtr<FPCGSplineStruct> SplineStruct = PCGExPaths::MakeSplineFromPoints(PathData->GetConstTransformValueRange(), Config.PointType, bIsClosedLoop, true))
			{
				Splines->Add(SplineStruct);
			}
		}
	}

	if (Splines->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); }
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPathInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPathInclusionFilter>(this);
}

void UPCGExPathInclusionFilterFactory::BeginDestroy()
{
	Splines.Reset();
	Super::BeginDestroy();
}

namespace PCGExPointFilter
{
	bool FPathInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		ToleranceSquared = FMath::Square(TypedFilterFactory->Config.Tolerance);

		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

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

	bool FPathInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		ESplineCheckFlags State = None;

		const TArray<TSharedPtr<FPCGSplineStruct>>& SplinesRef = *Splines.Get();
		const FVector Pos = Point.Transform.GetLocation();

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (const TSharedPtr<FPCGSplineStruct>& Spline : SplinesRef)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				const double D = FVector::DistSquared(Pos, TLoc);

				if (D > ClosestDist) { continue; }
				ClosestDist = D;

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { EnumAddFlags(State, On); }
				else { EnumRemoveFlags(State, On); }


				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
				{
					EnumAddFlags(State, Inside);
					EnumRemoveFlags(State, Outside);
				}
				else
				{
					EnumRemoveFlags(State, Inside);
					EnumAddFlags(State, Outside);
				}
			}
		}
		else
		{
			for (const TSharedPtr<FPCGSplineStruct>& Spline : SplinesRef)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared)
				{
					EnumAddFlags(State, On);
				}
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
				{
					EnumAddFlags(State, Inside);
				}
				else
				{
					EnumAddFlags(State, Outside);
				}
			}
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? EnumHasAnyFlags(State, GoodFlags) : EnumHasAllFlags(State, GoodFlags); } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}

	bool FPathInclusionFilter::Test(const int32 PointIndex) const
	{
		ESplineCheckFlags State = None;
		const TArray<TSharedPtr<FPCGSplineStruct>>& SplinesRef = *Splines.Get();

		const FVector Pos = InTransforms[PointIndex].GetLocation();

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (const TSharedPtr<FPCGSplineStruct>& Spline : SplinesRef)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				const double D = FVector::DistSquared(Pos, TLoc);

				if (D > ClosestDist) { continue; }
				ClosestDist = D;

				if (const FVector S = T.GetScale3D(); D < FVector2D(S.Y, S.Z).Length() * ToleranceSquared) { EnumAddFlags(State, On); }
				else { EnumRemoveFlags(State, On); }

				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
				{
					EnumAddFlags(State, Inside);
					EnumRemoveFlags(State, Outside);
				}
				else
				{
					EnumRemoveFlags(State, Inside);
					EnumAddFlags(State, Outside);
				}
			}
		}
		else
		{
			for (const TSharedPtr<FPCGSplineStruct>& Spline : SplinesRef)
			{
				const FTransform T = PCGExPaths::GetClosestTransform(Spline, Pos, TypedFilterFactory->Config.bSplineScalesTolerance);
				const FVector& TLoc = T.GetLocation();
				if (const FVector S = T.GetScale3D(); FVector::DistSquared(Pos, TLoc) < FVector2D(S.Y, S.Z).Length() * ToleranceSquared)
				{
					EnumAddFlags(State, On);
				}
				if (FVector::DotProduct(T.GetRotation().GetRightVector(), (TLoc - Pos).GetSafeNormal()) < TypedFilterFactory->Config.CurvatureThreshold)
				{
					EnumAddFlags(State, Inside);
				}
				else
				{
					EnumAddFlags(State, Outside);
				}
			}
		}

		bool bPass = (State & BadFlags) == 0;
		if (GoodMatch != Skip) { if (bPass) { bPass = GoodMatch == Any ? EnumHasAnyFlags(State, GoodFlags) : EnumHasAllFlags(State, GoodFlags); } }

		return TypedFilterFactory->Config.bInvert ? !bPass : bPass;
	}
}

TArray<FPCGPinProperties> UPCGExPathInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
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
