// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExMeanFilter.h"

PCGExPointFilter::TFilter* UPCGExMeanFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TMeanFilter(this);
}

bool PCGExPointsFilter::TMeanFilter::Init(const FPCGContext* InContext, PCGExDataCaching::FPool* InPointDataCache)
{
	if (!TFilter::Init(InContext, InPointDataCache)) { return false; }

	Target = PointDataCache->GetOrCreateGetter<double>(TypedFilterFactory->Descriptor.Target, true);

	if (!Target)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Target attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Target.GetName())));
		return false;
	}

	return true;
}

bool PCGExPointsFilter::TMeanFilter::Test(const int32 PointIndex) const
{
	return FMath::IsWithin(Target->Values[PointIndex], ReferenceMin, ReferenceMax);
}

void PCGExPointsFilter::TMeanFilter::PostInit()
{
	const int32 NumPoints = PointDataCache->Source->GetNum();
	Results.SetNum(NumPoints);

	double SumValue = 0;

	for (int i = 0; i < NumPoints; i++)
	{
		Results[i] = false;
		SumValue += Target->Values[i];
	}

	if (TypedFilterFactory->Descriptor.Measure == EPCGExMeanMeasure::Relative)
	{
		double RelativeMinEdgeLength = TNumericLimits<double>::Max();
		double RelativeMaxEdgeLength = TNumericLimits<double>::Min();
		SumValue = 0;
		for (int i = 0; i < NumPoints; i++)
		{
			const double Normalized = (Target->Values[i] /= Target->Max);
			RelativeMinEdgeLength = FMath::Min(Normalized, RelativeMinEdgeLength);
			RelativeMaxEdgeLength = FMath::Max(Normalized, RelativeMaxEdgeLength);
			SumValue += Normalized;
		}
		Target->Min = RelativeMinEdgeLength;
		Target->Max = 1;
	}

	switch (TypedFilterFactory->Descriptor.MeanMethod)
	{
	default:
	case EPCGExMeanMethod::Average:
		ReferenceValue = SumValue / NumPoints;
		break;
	case EPCGExMeanMethod::Median:
		ReferenceValue = PCGExMath::GetMedian(Target->Values);
		break;
	case EPCGExMeanMethod::Fixed:
		ReferenceValue = TypedFilterFactory->Descriptor.MeanValue;
		break;
	case EPCGExMeanMethod::ModeMin:
		ReferenceValue = PCGExMath::GetMode(Target->Values, false, TypedFilterFactory->Descriptor.ModeTolerance);
		break;
	case EPCGExMeanMethod::ModeMax:
		ReferenceValue = PCGExMath::GetMode(Target->Values, true, TypedFilterFactory->Descriptor.ModeTolerance);
		break;
	case EPCGExMeanMethod::Central:
		ReferenceValue = Target->Min + (Target->Max - Target->Min) * 0.5;
		break;
	}

	const double RMin = TypedFilterFactory->Descriptor.bDoExcludeBelowMean ? ReferenceValue - TypedFilterFactory->Descriptor.ExcludeBelow : 0;
	const double RMax = TypedFilterFactory->Descriptor.bDoExcludeAboveMean ? ReferenceValue + TypedFilterFactory->Descriptor.ExcludeAbove : TNumericLimits<double>::Max();

	ReferenceMin = FMath::Min(RMin, RMax);
	ReferenceMax = FMath::Max(RMin, RMax);
}

#define LOCTEXT_NAMESPACE "PCGExMeanFilterDefinition"
#define PCGEX_NAMESPACE MeanFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Mean)

#if WITH_EDITOR
FString UPCGExMeanFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "";

	if (Descriptor.bDoExcludeBelowMean) { DisplayName += FString::Printf(TEXT("< %.3f "), (static_cast<int32>(1000 * Descriptor.ExcludeBelow) / 1000.0)); }
	if (Descriptor.bDoExcludeBelowMean && Descriptor.bDoExcludeAboveMean) { DisplayName += "&& "; }
	if (Descriptor.bDoExcludeAboveMean) { DisplayName += FString::Printf(TEXT("> %.3f "), (static_cast<int32>(1000 * Descriptor.ExcludeAbove) / 1000.0)); }

	DisplayName += Descriptor.Target.GetName().ToString() + "' ";

	switch (Descriptor.MeanMethod)
	{
	case EPCGExMeanMethod::Average:
		DisplayName += "' Average";
		break;
	case EPCGExMeanMethod::Median:
		DisplayName += "' Median";
		break;
	case EPCGExMeanMethod::ModeMin:
		DisplayName += "' Mode (min)";
		break;
	case EPCGExMeanMethod::ModeMax:
		DisplayName += "' Mode (max)";
		break;
	case EPCGExMeanMethod::Central:
		DisplayName += "' Central";
		break;
	case EPCGExMeanMethod::Fixed:
		DisplayName += FString::Printf(TEXT(" %.3f"), (static_cast<int32>(1000 * Descriptor.MeanValue) / 1000.0));
		break;
	default: ;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
