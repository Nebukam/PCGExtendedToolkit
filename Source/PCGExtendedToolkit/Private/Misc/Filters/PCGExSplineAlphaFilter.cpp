// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExSplineAlphaFilter.h"


#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExSplineAlphaFilterDefinition"
#define PCGEX_NAMESPACE PCGExSplineAlphaFilterDefinition

bool UPCGExSplineAlphaFilterFactory::SupportsPointEvaluation() const
{
	return Config.CompareAgainst == EPCGExInputValueType::Constant;
}

bool UPCGExSplineAlphaFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	return true;
}

bool UPCGExSplineAlphaFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExSplineAlphaFilterFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	Splines = MakeShared<TArray<const FPCGSplineStruct*>>();
	SegmentsNum = MakeShared<TArray<double>>();

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
		!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData || SplineData->SplineStruct.GetNumberOfSplineSegments() <= 0) { continue; }

			const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
			if (Config.SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

			Splines->Add(&SplineData->SplineStruct);
			SegmentsNum->Add(SplineData->SplineStruct.GetNumberOfSplineSegments());
		}
	}

	if (Splines->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); }
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExSplineAlphaFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FSplineAlphaFilter>(this);
}

void UPCGExSplineAlphaFilterFactory::BeginDestroy()
{
	Splines.Reset();
	SegmentsNum.Reset();
	Super::BeginDestroy();
}

bool UPCGExSplineAlphaFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

namespace PCGExPointFilter
{
	bool FSplineAlphaFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

		OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
		if (!OperandB->Init(InContext, PointDataFacade)) { return false; }

		return true;
	}

	bool FSplineAlphaFilter::Test(const FPCGPoint& Point) const
	{
		const FVector Pos = Point.Transform.GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct* Spline = *(Splines->GetData() + i);

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				FTransform T = Spline->GetTransformAtSplineInputKey(static_cast<float>(LocalTime), ESplineCoordinateSpace::World, true);
				LocalTime /= *(SegmentsNum->GetData() + i);

				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				Time = LocalTime;
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct* Spline = *(Splines->GetData() + i);

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				LocalTime /= *(SegmentsNum->GetData() + i);

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
				Time /= SegmentsNum->Num();
			}
		}

		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Time, TypedFilterFactory->Config.OperandBConstant, TypedFilterFactory->Config.Tolerance);
	}

	bool FSplineAlphaFilter::Test(const int32 PointIndex) const
	{
		const FVector Pos = PointDataFacade->Source->GetInPoint(PointIndex).Transform.GetLocation();
		double Time = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Time = MAX_dbl; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double ClosestDist = MAX_dbl;
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct* Spline = *(Splines->GetData() + i);

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				FTransform T = Spline->GetTransformAtSplineInputKey(static_cast<float>(LocalTime), ESplineCoordinateSpace::World, true);
				LocalTime /= *(SegmentsNum->GetData() + i);

				const double D = FVector::DistSquared(T.GetLocation(), Pos);

				if (D > ClosestDist) { continue; }

				Time = LocalTime;
			}
		}
		else
		{
			for (int i = 0; i < Splines->Num(); i++)
			{
				const FPCGSplineStruct* Spline = *(Splines->GetData() + i);

				double LocalTime = Spline->FindInputKeyClosestToWorldLocation(Pos);
				LocalTime /= *(SegmentsNum->GetData() + i);

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
				Time /= SegmentsNum->Num();
			}
		}

		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Time, OperandB->Read(PointIndex), TypedFilterFactory->Config.Tolerance);
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
