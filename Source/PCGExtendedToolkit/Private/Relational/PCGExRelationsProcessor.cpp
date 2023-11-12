// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Relational\PCGExRelationsProcessor.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExRelationalSettings"

#pragma region UPCGSettings interface

namespace PCGExRelational
{
	const FName SourceRelationalParamsLabel = TEXT("RelationalParams");
}

#if WITH_EDITOR

FText UPCGExRelationsProcessorSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExRelationsProcessorTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExRelationsProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertyParams = PinProperties.Emplace_GetRef(PCGExRelational::SourceRelationalParamsLabel, EPCGDataType::Param);

#if WITH_EDITOR
	PinPropertyParams.Tooltip = LOCTEXT("PCGExSourceRelationalParamsPinTooltip", "Relations Params.");
#endif // WITH_EDITOR

	return PinProperties;
}

bool FPCGExRelationsProcessorContext::AdvanceParams(bool bResetPointsIndex)
{
	if (bResetPointsIndex) { CurrentPointsIndex = -1; }
	CurrentParamsIndex++;
	if (Params.Params.IsValidIndex(CurrentParamsIndex))
	{
		//UE_LOG(LogTemp, Warning, TEXT("AdvanceParams to %d"), CurrentParamsIndex);
		CurrentParams = Params.Params[CurrentParamsIndex];
		return true;
	}

	CurrentParams = nullptr;
	return false;
}

bool FPCGExRelationsProcessorContext::AdvancePointsIO(bool bResetParamsIndex)
{
	if (bResetParamsIndex) { CurrentParamsIndex = -1; }
	return FPCGExPointsProcessorContext::AdvancePointsIO();
}

void FPCGExRelationsProcessorContext::Reset()
{
	FPCGExPointsProcessorContext::Reset();
	CurrentParamsIndex = -1;
}

bool FPCGExRelationsProcessorContext::IsValid()
{
	return FPCGExPointsProcessorContext::IsValid() && !Params.IsEmpty();
}

FPCGContext* FPCGExRelationsProcessorElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExRelationsProcessorContext* Context = new FPCGExRelationsProcessorContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExRelationsProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	FPCGExRelationsProcessorContext* Context = static_cast<FPCGExRelationsProcessorContext*>(InContext);
		
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	Context->Params.Initialize(InContext, Sources);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
