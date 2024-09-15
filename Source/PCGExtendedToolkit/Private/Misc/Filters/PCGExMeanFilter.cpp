// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExMeanFilter.h"

#define LOCTEXT_NAMESPACE "PCGExMeanFilterDefinition"
#define PCGEX_NAMESPACE MeanFilterDefinition

PCGExPointFilter::TFilter* UPCGExMeanFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TMeanFilter(this);
}

bool PCGExPointsFilter::TMeanFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	const PCGExData::TCache<double>* Target = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.Target, true);

	if (!Target)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Target attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.Target.GetName())));
		return false;
	}

	DataMin = Target->Min;
	DataMax = Target->Max;

	Values.Reserve(Target->Values.Num());
	Values.Append(Target->Values);

	return true;
}

void PCGExPointsFilter::TMeanFilter::PostInit()
{
	const int32 NumPoints = PointDataFacade->Source->GetNum();
	Results.Init(false, NumPoints);

	double SumValue = 0;
	for (int i = 0; i < NumPoints; ++i) { SumValue += Values[i]; }

	if (TypedFilterFactory->Config.Measure == EPCGExMeanMeasure::Relative)
	{
		double RelativeMinEdgeLength = TNumericLimits<double>::Max();
		double RelativeMaxEdgeLength = TNumericLimits<double>::Min();
		SumValue = 0;
		for (int i = 0; i < NumPoints; ++i)
		{
			const double Normalized = (Values[i] /= DataMax);
			RelativeMinEdgeLength = FMath::Min(Normalized, RelativeMinEdgeLength);
			RelativeMaxEdgeLength = FMath::Max(Normalized, RelativeMaxEdgeLength);
			SumValue += Normalized;
		}
		DataMin = RelativeMinEdgeLength;
		DataMax = RelativeMaxEdgeLength;
	}

	switch (TypedFilterFactory->Config.MeanMethod)
	{
	default:
	case EPCGExMeanMethod::Average:
		ReferenceValue = SumValue / NumPoints;
		break;
	case EPCGExMeanMethod::Median:
		ReferenceValue = PCGExMath::GetMedian(Values);
		break;
	case EPCGExMeanMethod::Fixed:
		ReferenceValue = TypedFilterFactory->Config.MeanValue;
		break;
	case EPCGExMeanMethod::ModeMin:
		ReferenceValue = PCGExMath::GetMode(Values, false, TypedFilterFactory->Config.ModeTolerance);
		break;
	case EPCGExMeanMethod::ModeMax:
		ReferenceValue = PCGExMath::GetMode(Values, true, TypedFilterFactory->Config.ModeTolerance);
		break;
	case EPCGExMeanMethod::Central:
		ReferenceValue = DataMin + (DataMax - DataMin) * 0.5;
		break;
	}

	const double RMin = TypedFilterFactory->Config.bDoExcludeBelowMean ? ReferenceValue - TypedFilterFactory->Config.ExcludeBelow : TNumericLimits<double>::Min();
	const double RMax = TypedFilterFactory->Config.bDoExcludeAboveMean ? ReferenceValue + TypedFilterFactory->Config.ExcludeAbove : TNumericLimits<double>::Max();

	ReferenceMin = FMath::Min(RMin, RMax);
	ReferenceMax = FMath::Max(RMin, RMax);
}

PCGEX_CREATE_FILTER_FACTORY(Mean)

#if WITH_EDITOR
FString UPCGExMeanFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "";

	if (Config.bDoExcludeBelowMean) { DisplayName += FString::Printf(TEXT("< %.3f "), (static_cast<int32>(1000 * Config.ExcludeBelow) / 1000.0)); }
	if (Config.bDoExcludeBelowMean && Config.bDoExcludeAboveMean) { DisplayName += "&& "; }
	if (Config.bDoExcludeAboveMean) { DisplayName += FString::Printf(TEXT("> %.3f "), (static_cast<int32>(1000 * Config.ExcludeAbove) / 1000.0)); }

	DisplayName += Config.Target.GetName().ToString() + "' ";

	switch (Config.MeanMethod)
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
		DisplayName += FString::Printf(TEXT(" %.3f"), (static_cast<int32>(1000 * Config.MeanValue) / 1000.0));
		break;
	default: ;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
