// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPathAlphaFilter.h"

#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExPathAlphaFilterDefinition

bool UPCGExPathAlphaFilterFactory::Init(FPCGExContext* InContext)
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

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPathAlphaFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TPathAlphaFilter>(this);
}

void UPCGExPathAlphaFilterFactory::BeginDestroy()
{
	Splines.Empty();
	Super::BeginDestroy();
}

bool UPCGExPathAlphaFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

namespace PCGExPointsFilter
{
	bool TPathAlphaFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
		{
			OperandB = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandB);

			if (!OperandB)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
				return false;
			}
		}

		return true;
	}

	bool TPathAlphaFilter::Test(const int32 PointIndex) const
	{
		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines.Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = Splines[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				FTransform T = Spline->GetTransformAtSplineInputKey(static_cast<float>(LocalTime), ESplineCoordinateSpace::World, true);
				LocalTime /= SegmentsNum[i];

				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				Time = LocalTime;
			}
		}
		else
		{
			for (int i = 0; i < Splines.Num(); i++)
			{
				const TSharedPtr<const FPCGSplineStruct> Spline = Splines[i];

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				LocalTime /= SegmentsNum[i];

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
				Time /= SegmentsNum.Num();
			}
		}

		const double B = OperandB ? OperandB->Read(PointIndex) : TypedFilterFactory->Config.OperandBConstant;
		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Time, B, TypedFilterFactory->Config.Tolerance);
	}
}

TArray<FPCGPinProperties> UPCGExPathAlphaFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
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
