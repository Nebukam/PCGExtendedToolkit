// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineAlphaFilter.h"

#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExSplineAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineAlphaFilterDefinition

bool UPCGExSplineAlphaFilterFactory::Init(FPCGExContext* InContext)
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
			SegmentsNum.Add(SplineData->SplineStruct.GetNumberOfSplineSegments());
		}
	}

	if (Splines.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (either no input or empty dataset)"));
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExSplineAlphaFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TSplineAlphaFilter>(this);
}

void UPCGExSplineAlphaFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

bool UPCGExSplineAlphaFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

namespace PCGExPointsFilter
{
	bool TSplineAlphaFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
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

	bool TSplineAlphaFilter::Test(const int32 PointIndex) const
	{
		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines.Num(); i++)
			{
				const FPCGSplineStruct* Spline = Splines[i];

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
				const FPCGSplineStruct* Spline = Splines[i];

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

TArray<FPCGPinProperties> UPCGExSplineAlphaFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(FName("Splines"), TEXT("Splines will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(SplineAlpha)

#if WITH_EDITOR
FString UPCGExSplineAlphaFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Alpha ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
