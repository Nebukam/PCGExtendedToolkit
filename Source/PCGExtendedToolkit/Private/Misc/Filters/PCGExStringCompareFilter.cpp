// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExStringCompareFilter.h"

#if WITH_EDITOR
FString FPCGExStringCompareFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString();

	switch (Comparison)
	{
	case EPCGExStringComparison::StrictlyEqual:
		DisplayName += " == ";
		break;
	case EPCGExStringComparison::StrictlyNotEqual:
		DisplayName += " != ";
		break;
	case EPCGExStringComparison::LengthStrictlyEqual:
		DisplayName += " L == L ";
		break;
	case EPCGExStringComparison::LengthStrictlyUnequal:
		DisplayName += " L != L ";
		break;
	case EPCGExStringComparison::LengthEqualOrGreater:
		DisplayName += " L >= L ";
		break;
	case EPCGExStringComparison::LengthEqualOrSmaller:
		DisplayName += " L <= L ";
		break;
	case EPCGExStringComparison::StrictlyGreater:
		DisplayName += " L > L ";
		break;
	case EPCGExStringComparison::StrictlySmaller:
		DisplayName += " L < L ";
		break;
	case EPCGExStringComparison::LocaleStrictlyGreater:
		DisplayName += " > ";
		break;
	case EPCGExStringComparison::LocaleStrictlySmaller:
		DisplayName += " < ";
		break;
	default: ;
	}

	DisplayName += OperandB.GetName().ToString();
	return DisplayName;
}
#endif

PCGExDataFilter::TFilterHandler* UPCGExStringCompareFilterDefinition::CreateHandler() const
{
	return new PCGExPointsFilter::TStringComparisonHandler(this);
}

void UPCGExStringCompareFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

void PCGExPointsFilter::TStringComparisonHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::TFAttributeReader<FString>(CompareFilter->OperandA.GetName());
	bValid = OperandA->Bind(*const_cast<PCGExData::FPointIO*>(PointIO));

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(CompareFilter->OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (CompareFilter->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::TFAttributeReader<FString>(CompareFilter->OperandB.GetName());
		bValid = OperandB->Bind(*const_cast<PCGExData::FPointIO*>(PointIO));

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(CompareFilter->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TStringComparisonHandler::Test(const int32 PointIndex) const
{
	const FString A = OperandA->Values[PointIndex];
	const FString B = CompareFilter->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : CompareFilter->OperandBConstant;

	switch (CompareFilter->Comparison)
	{
	case EPCGExStringComparison::StrictlyEqual:
		return A == B;
	case EPCGExStringComparison::StrictlyNotEqual:
		return A != B;
	case EPCGExStringComparison::LengthStrictlyEqual:
		return A.Len() == B.Len();
	case EPCGExStringComparison::LengthStrictlyUnequal:
		return A.Len() != B.Len();
	case EPCGExStringComparison::LengthEqualOrGreater:
		return A.Len() >= B.Len();
	case EPCGExStringComparison::LengthEqualOrSmaller:
		return A.Len() <= B.Len();
	case EPCGExStringComparison::StrictlyGreater:
		return A.Len() > B.Len();
	case EPCGExStringComparison::StrictlySmaller:
		return A.Len() < B.Len();
	case EPCGExStringComparison::LocaleStrictlyGreater:
		return A > B;
	case EPCGExStringComparison::LocaleStrictlySmaller:
		return A < B;
	default:
		return false;
	}
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

FPCGElementPtr UPCGExStringCompareFilterDefinitionSettings::CreateElement() const { return MakeShared<FPCGExStringCompareFilterDefinitionElement>(); }

TArray<FPCGPinProperties> UPCGExStringCompareFilterDefinitionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExStringCompareFilterDefinitionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExDataFilter::OutputFilterLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single filter definition.");
#endif

	return PinProperties;
}

bool FPCGExStringCompareFilterDefinitionElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCompareFilterDefinitionElement::Execute);

	PCGEX_SETTINGS(StringCompareFilterDefinition)

	UPCGExStringCompareFilterDefinition* OutFilter = NewObject<UPCGExStringCompareFilterDefinition>();

	OutFilter->Priority = 0;
	OutFilter->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutFilter;
	Output.Pin = PCGExDataFilter::OutputFilterLabel;

	return true;
}

FPCGContext* FPCGExStringCompareFilterDefinitionElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
