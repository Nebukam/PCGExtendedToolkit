// Fill out your copyright notice in the Description page of Project Settings.

#include "PCGExPointsProcessor.h"

#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

#pragma region UPCGSettings interface

namespace PCGEx
{
	const FName SourcePointsLabel = TEXT("Source");
	const FName OutputPointsLabel = TEXT("Points");
}

#if WITH_EDITOR

FText UPCGExPointsProcessorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGEx::SourcePointsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip", "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPointsProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(PCGEx::OutputPointsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = LOCTEXT("PCGExOutputPointsPinTooltip", "The source points.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGEx::EIOInit UPCGExPointsProcessorSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NewOutput; }

bool FPCGExPointsProcessorContext::AdvancePointsIO()
{
	CurrentPointsIndex++;
	if (Points.Pairs.IsValidIndex(CurrentPointsIndex))
	{
		CurrentIO = &(Points.Pairs[CurrentPointsIndex]);
		return true;
	}
	CurrentIO = nullptr;
	return false;
}

void FPCGExPointsProcessorContext::SetOperation(int32 OperationId)
{
	int32 PreviousOperation = CurrentOperation;
	CurrentOperation = OperationId;
}

void FPCGExPointsProcessorContext::Reset() { CurrentOperation = -1; }

bool FPCGExPointsProcessorContext::IsValid() { return !Points.IsEmpty(); }

#pragma endregion

FPCGContext* FPCGExPointsProcessorElementBase::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExPointsProcessorContext* Context = new FPCGExPointsProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExPointsProcessorElementBase::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	InContext->InputData = InputData;
	InContext->SourceComponent = SourceComponent;
	InContext->Node = Node;

	const UPCGExPointsProcessorSettings* Settings = InContext->GetInputSettings<UPCGExPointsProcessorSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = InContext->InputData.GetInputsByPin(PCGEx::SourcePointsLabel);
	InContext->Points.Initialize(InContext, Sources, Settings->GetPointOutputInitMode());
}

#undef LOCTEXT_NAMESPACE
