// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataEventDispatch.h"

#include "PCGGraph.h"
#include "PCGPin.h"
#include "UPCGExSubSystem.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE DataEventDispatch

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExDataEventDispatchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::SourcePointsLabel, "In.", Required, {})
	PCGEX_PIN_DEPENDENCIES
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExDataEventDispatchSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::OutputPointsLabel, "Same as in.", Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExDataEventDispatchSettings::CreateElement() const { return MakeShared<FPCGExDataEventDispatchElement>(); }

#pragma endregion

FPCGContext* FPCGExDataEventDispatchElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDataEventDispatchContext* Context = new FPCGExDataEventDispatchContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExDataEventDispatchElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(DataEventDispatch)

	if (Settings->Scope == EPCGExEventScope::Owner)
	{
		Context->SourceComponent->GetWorld()->GetSubsystem<UPCGExSubSystem>()->Dispatch(
			InContext->SourceComponent.Get(),
			Context->InputData.GetInputsByPin(PCGEx::SourcePointsLabel),
			PCGEx::FPCGExEvent(EPCGExEventScope::Owner, Settings->Event, Context->SourceComponent->GetOwner()));
	}
	else
	{
		Context->SourceComponent->GetWorld()->GetSubsystem<UPCGExSubSystem>()->Dispatch(
			InContext->SourceComponent.Get(),
			Context->InputData.GetInputsByPin(PCGEx::SourcePointsLabel),
			PCGEx::FPCGExEvent(EPCGExEventScope::Global, Settings->Event));
	}


	Context->OutputData = Context->InputData; // Full pass-through

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
