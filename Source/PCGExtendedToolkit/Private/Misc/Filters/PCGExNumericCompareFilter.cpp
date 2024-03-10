// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"

#if WITH_EDITOR
FString FPCGExNumericCompareFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString();

	switch (Comparison)
	{
	case EPCGExComparison::StrictlyEqual:
		DisplayName += " == ";
		break;
	case EPCGExComparison::StrictlyNotEqual:
		DisplayName += " != ";
		break;
	case EPCGExComparison::EqualOrGreater:
		DisplayName += " >= ";
		break;
	case EPCGExComparison::EqualOrSmaller:
		DisplayName += " <= ";
		break;
	case EPCGExComparison::StrictlyGreater:
		DisplayName += " > ";
		break;
	case EPCGExComparison::StrictlySmaller:
		DisplayName += " < ";
		break;
	case EPCGExComparison::NearlyEqual:
		DisplayName += " ~= ";
		break;
	case EPCGExComparison::NearlyNotEqual:
		DisplayName += " !~= ";
		break;
	default: DisplayName += " ?? ";
	}

	switch (CompareAgainst)
	{
	default:
	case EPCGExOperandType::Attribute:
		DisplayName += OperandB.GetName().ToString();
		break;
	case EPCGExOperandType::Constant:
		DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * OperandBConstant) / 1000.0));
		break;
	}

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

void PCGExPointsFilter::TNumericComparisonHandler::Capture(const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::FLocalSingleFieldGetter();
	OperandA->Capture(CompareFilter->OperandA);
	OperandA->Grab(*PointIO, false);
	if (!OperandA->IsUsable(PointIO->GetNum())) { bValid = false; }

	if (CompareFilter->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(CompareFilter->OperandB);
		OperandB->Grab(*PointIO, false);
		if (!OperandB->IsUsable(PointIO->GetNum())) { bValid = false; }
	}

	if (!bValid)
	{
		PCGEX_DELETE(OperandA)
		PCGEX_DELETE(OperandB)
	}
}

bool PCGExPointsFilter::TNumericComparisonHandler::Test(const int32 PointIndex) const
{
	const double A = OperandA->Values[PointIndex];
	const double B = CompareFilter->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : CompareFilter->OperandBConstant;

	switch (CompareFilter->Comparison)
	{
	case EPCGExComparison::StrictlyEqual:
		return A == B;
	case EPCGExComparison::StrictlyNotEqual:
		return A != B;
	case EPCGExComparison::EqualOrGreater:
		return A >= B;
	case EPCGExComparison::EqualOrSmaller:
		return A <= B;
	case EPCGExComparison::StrictlyGreater:
		return A > B;
	case EPCGExComparison::StrictlySmaller:
		return A < B;
	case EPCGExComparison::NearlyEqual:
		return FMath::IsNearlyEqual(A, B, CompareFilter->Tolerance);
	case EPCGExComparison::NearlyNotEqual:
		return !FMath::IsNearlyEqual(A, B, CompareFilter->Tolerance);
	default: return false;
	}
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
