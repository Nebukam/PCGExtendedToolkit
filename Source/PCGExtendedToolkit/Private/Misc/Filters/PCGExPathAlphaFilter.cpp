// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathAlphaFilter.h"




#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathAlphaFilterDefinition

bool UPCGExPathAlphaFilterFactory::SupportsProxyEvaluation() const
{
	return Config.CompareAgainst == EPCGExInputValueType::Constant;
}

bool UPCGExPathAlphaFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

bool UPCGExPathAlphaFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExPathAlphaFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	if (!Super::Prepare(InContext, AsyncManager)) { return false; }

	Splines = MakeShared<TArray<TSharedPtr<FPCGSplineStruct>>>();

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::SourcePathsLabel);
		!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data);
			if (!PathData) { continue; }

			const bool bIsClosedLoop = PCGExPaths::GetClosedLoop(PathData);
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
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExPathAlphaFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPathAlphaFilter>(this);
}

void UPCGExPathAlphaFilterFactory::BeginDestroy()
{
	Splines.Reset();
	SegmentsNum.Reset();
	Super::BeginDestroy();
}

bool UPCGExPathAlphaFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

namespace PCGExPointFilter
{
	bool FPathAlphaFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

		OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
		if (!OperandB->Init(InContext, PointDataFacade)) { return false; }

		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

		return true;
	}

	bool FPathAlphaFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		const TArray<TSharedPtr<FPCGSplineStruct>>& SplinesRef = *Splines.Get();
		const TArray<double>& SegmentsNumRef = *SegmentsNum.Get();

		const FVector Pos = Point.Transform.GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = SplinesRef[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				FTransform T = Spline->GetTransformAtSplineInputKey(static_cast<float>(LocalTime), ESplineCoordinateSpace::World, true);
				LocalTime /= SegmentsNumRef[i];

				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				Time = LocalTime;
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = SplinesRef[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				LocalTime /= SegmentsNumRef[i];

				switch (TypedFilterFactory->Config.TimeConsolidation)
				{
				case EPCGExSplineTimeConsolidation::Min:
					Time = FMath::Min(LocalTime, Time);
					break;
				case EPCGExSplineTimeConsolidation::Max:
					Time = FMath::Max(LocalTime, Time);
					break;
				case EPCGExSplineTimeConsolidation::Average:
					Time += LocalTime;
					break;
				}
			}

			if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Average)
			{
				Time /= SegmentsNumRef.Num();
			}
		}

		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Time, TypedFilterFactory->Config.OperandBConstant, TypedFilterFactory->Config.Tolerance);
	}

	bool FPathAlphaFilter::Test(const int32 PointIndex) const
	{
		const TArray<TSharedPtr<FPCGSplineStruct>>& SplinesRef = *Splines.Get();
		const TArray<double>& SegmentsNumRef = *SegmentsNum.Get();

		const FVector Pos = InTransforms[PointIndex].GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = SplinesRef[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				FTransform T = Spline->GetTransformAtSplineInputKey(static_cast<float>(LocalTime), ESplineCoordinateSpace::World, true);
				LocalTime /= SegmentsNumRef[i];

				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				Time = LocalTime;
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = SplinesRef[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				LocalTime /= SegmentsNumRef[i];

				switch (TypedFilterFactory->Config.TimeConsolidation)
				{
				case EPCGExSplineTimeConsolidation::Min:
					Time = FMath::Min(LocalTime, Time);
					break;
				case EPCGExSplineTimeConsolidation::Max:
					Time = FMath::Max(LocalTime, Time);
					break;
				case EPCGExSplineTimeConsolidation::Average:
					Time += LocalTime;
					break;
				}
			}

			if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Average)
			{
				Time /= SegmentsNumRef.Num();
			}
		}

		const double B = OperandB ? OperandB->Read(PointIndex) : TypedFilterFactory->Config.OperandBConstant;
		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Time, B, TypedFilterFactory->Config.Tolerance);
	}
}

TArray<FPCGPinProperties> UPCGExPathAlphaFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(PathAlpha)

#if WITH_EDITOR
FString UPCGExPathAlphaFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Alpha ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
