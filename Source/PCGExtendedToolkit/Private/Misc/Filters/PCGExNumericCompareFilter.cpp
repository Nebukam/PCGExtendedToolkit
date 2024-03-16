// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"

#if WITH_EDITOR
FString FPCGExNumericCompareFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString() + PCGExCompare::ToString(Comparison);

	if (CompareAgainst == EPCGExOperandType::Attribute) { DisplayName += OperandB.GetName().ToString(); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

PCGExDataFilter::TFilterHandler* UPCGExNumericCompareFilterDefinition::CreateHandler() const
{
	return new PCGExPointsFilter::TNumericComparisonHandler(this);
}

void UPCGExNumericCompareFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

void PCGExPointsFilter::TNumericComparisonHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::FLocalSingleFieldGetter();
	OperandA->Capture(CompareFilter->OperandA);
	OperandA->Grab(*PointIO, false);
	bValid = OperandA->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(CompareFilter->OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (CompareFilter->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(CompareFilter->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(CompareFilter->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TNumericComparisonHandler::Test(const int32 PointIndex) const
{
	const double A = OperandA->Values[PointIndex];
	const double B = CompareFilter->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : CompareFilter->OperandBConstant;
	return PCGExCompare::Compare(CompareFilter->Comparison, A, B, CompareFilter->Tolerance);
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

FPCGElementPtr UPCGExNumericCompareFilterDefinitionSettings::CreateElement() const { return MakeShared<FPCGExNumericCompareFilterDefinitionElement>(); }

TArray<FPCGPinProperties> UPCGExNumericCompareFilterDefinitionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExNumericCompareFilterDefinitionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExDataFilter::OutputFilterLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single filter definition.");
#endif

	return PinProperties;
}

bool FPCGExNumericCompareFilterDefinitionElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCompareFilterDefinitionElement::Execute);

	PCGEX_SETTINGS(NumericCompareFilterDefinition)

	UPCGExNumericCompareFilterDefinition* OutFilter = NewObject<UPCGExNumericCompareFilterDefinition>();

	OutFilter->Priority = 0;
	OutFilter->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutFilter;
	Output.Pin = PCGExDataFilter::OutputFilterLabel;

	return true;
}

FPCGContext* FPCGExNumericCompareFilterDefinitionElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
