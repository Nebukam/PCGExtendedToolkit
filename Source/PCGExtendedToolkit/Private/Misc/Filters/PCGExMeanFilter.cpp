// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExMeanFilter.h"

#if WITH_EDITOR
FString FPCGExMeanFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = "";

	if (bDoExcludeBelowMean) { DisplayName += FString::Printf(TEXT("< %.3f "), (static_cast<int32>(1000 * ExcludeBelow) / 1000.0)); }
	if (bDoExcludeBelowMean && bDoExcludeAboveMean) { DisplayName += "&& "; }
	if (bDoExcludeAboveMean) { DisplayName += FString::Printf(TEXT("> %.3f "), (static_cast<int32>(1000 * ExcludeAbove) / 1000.0)); }

	DisplayName += Target.GetName().ToString() + "' ";

	switch (MeanMethod)
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
		DisplayName += FString::Printf(TEXT(" %.3f"), (static_cast<int32>(1000 * MeanValue) / 1000.0));
		break;
	default: ;
	}

	return DisplayName;
}
#endif

PCGExDataFilter::TFilterHandler* UPCGExMeanFilterDefinition::CreateHandler() const
{
	return new PCGExPointsFilter::TMeanHandler(this);
}

void UPCGExMeanFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

void PCGExPointsFilter::TMeanHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	Target = new PCGEx::FLocalSingleFieldGetter();

	Target->Capture(MeanFilter->Target);
	Target->Grab(*PointIO, true);
	bValid = Target->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Target attribute: {0}."), FText::FromName(MeanFilter->Target.GetName())));
		PCGEX_DELETE(Target)
	}
}

bool PCGExPointsFilter::TMeanHandler::Test(const int32 PointIndex) const
{
	return FMath::IsWithin(Target->Values[PointIndex], ReferenceMin, ReferenceMax);
}

void PCGExPointsFilter::TMeanHandler::PrepareForTesting(PCGExData::FPointIO* PointIO)
{
	const int32 NumPoints = PointIO->GetNum();
	Results.SetNum(NumPoints);

	double SumValue = 0;

	for (int i = 0; i < NumPoints; i++)
	{
		Results[i] = false;
		SumValue += Target->Values[i];
	}

	if (MeanFilter->Measure == EPCGExMeanMeasure::Relative)
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

	switch (MeanFilter->MeanMethod)
	{
	default:
	case EPCGExMeanMethod::Average:
		ReferenceValue = SumValue / NumPoints;
		break;
	case EPCGExMeanMethod::Median:
		ReferenceValue = PCGExMath::GetMedian(Target->Values);
		break;
	case EPCGExMeanMethod::Fixed:
		ReferenceValue = MeanFilter->MeanValue;
		break;
	case EPCGExMeanMethod::ModeMin:
		ReferenceValue = PCGExMath::GetMode(Target->Values, false, MeanFilter->ModeTolerance);
		break;
	case EPCGExMeanMethod::ModeMax:
		ReferenceValue = PCGExMath::GetMode(Target->Values, true, MeanFilter->ModeTolerance);
		break;
	case EPCGExMeanMethod::Central:
		ReferenceValue = Target->Min + (Target->Max - Target->Min) * 0.5;
		break;
	}

	const double RMin = MeanFilter->bPruneBelowMean ? ReferenceValue - MeanFilter->PruneBelow : 0;
	const double RMax = MeanFilter->bPruneAboveMean ? ReferenceValue + MeanFilter->PruneAbove : TNumericLimits<double>::Max();

	ReferenceMin = FMath::Min(RMin, RMax);
	ReferenceMax = FMath::Max(RMin, RMax);
}

#define LOCTEXT_NAMESPACE "PCGExMeanFilterDefinition"
#define PCGEX_NAMESPACE MeanFilterDefinition

FPCGElementPtr UPCGExMeanFilterDefinitionSettings::CreateElement() const { return MakeShared<FPCGExMeanFilterDefinitionElement>(); }

TArray<FPCGPinProperties> UPCGExMeanFilterDefinitionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExMeanFilterDefinitionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExDataFilter::OutputFilterLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single filter definition.");
#endif

	return PinProperties;
}

bool FPCGExMeanFilterDefinitionElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMeanFilterDefinitionElement::Execute);

	PCGEX_SETTINGS(MeanFilterDefinition)

	UPCGExMeanFilterDefinition* OutFilter = NewObject<UPCGExMeanFilterDefinition>();

	OutFilter->Priority = 0;
	OutFilter->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutFilter;
	Output.Pin = PCGExDataFilter::OutputFilterLabel;

	return true;
}

FPCGContext* FPCGExMeanFilterDefinitionElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
