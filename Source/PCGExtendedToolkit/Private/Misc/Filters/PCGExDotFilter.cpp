// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

#if WITH_EDITOR
FString FPCGExDotFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString() + " . ";

	if (CompareAgainst == EPCGExOperandType::Attribute) { DisplayName += OperandB.GetName().ToString(); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}
#endif

PCGExDataFilter::TFilterHandler* UPCGExDotFilterDefinition::CreateHandler() const
{
	return new PCGExPointsFilter::TDotHandler(this);
}

void UPCGExDotFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

void PCGExPointsFilter::TDotHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::FLocalVectorGetter();
	OperandA->Capture(DotFilter->OperandA);
	OperandA->Grab(*PointIO, false);
	bValid = OperandA->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(DotFilter->OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (DotFilter->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalVectorGetter();
		OperandB->Capture(DotFilter->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(DotFilter->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TDotHandler::Test(const int32 PointIndex) const
{
	const FVector A = OperandA->Values[PointIndex];
	const FVector B = DotFilter->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : DotFilter->OperandBConstant;

	const double Dot = DotFilter->bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);

	if (DotFilter->bExcludeAboveDot && Dot > DotFilter->ExcludeAbove) { return false; }
	if (DotFilter->bExcludeBelowDot && Dot < DotFilter->ExcludeBelow) { return false; }

	return true;
}

#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE DotFilterDefinition

FPCGElementPtr UPCGExDotFilterDefinitionSettings::CreateElement() const { return MakeShared<FPCGExDotFilterDefinitionElement>(); }

TArray<FPCGPinProperties> UPCGExDotFilterDefinitionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExDotFilterDefinitionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGExDataFilter::OutputFilterLabel, EPCGDataType::Param, false, false);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = FTEXT("Outputs a single filter definition.");
#endif

	return PinProperties;
}

bool FPCGExDotFilterDefinitionElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDotFilterDefinitionElement::Execute);

	PCGEX_SETTINGS(DotFilterDefinition)

	UPCGExDotFilterDefinition* OutFilter = NewObject<UPCGExDotFilterDefinition>();

	OutFilter->Priority = 0;
	OutFilter->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutFilter;
	Output.Pin = PCGExDataFilter::OutputFilterLabel;

	return true;
}

FPCGContext* FPCGExDotFilterDefinitionElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
