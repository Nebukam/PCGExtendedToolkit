// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataEventListen.h"

#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE DataEventListen

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExDataEventListenSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExDataEventListenSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::OutputPointsLabel, "Out.", Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExDataEventListenSettings::CreateElement() const { return MakeShared<FPCGExDataEventListenElement>(); }

#pragma endregion

FPCGContext* FPCGExDataEventListenElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDataEventListenContext* Context = new FPCGExDataEventListenContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExDataEventListenElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(DataEventListen)

	TWeakPtr<FPCGContextHandle> CtxHandle = Context->GetOrCreateHandle();
	auto EventCallback = [CtxHandle]()
	{
		FPCGExDataEventListenContext* Ctx = FPCGExContext::GetContextFromHandle<FPCGExDataEventListenContext>(CtxHandle);
		if (!Ctx) { return; }
		Ctx->bIsPaused = false;
	};

	if (!Context->bListening)
	{
		Context->bListening = true;
		Context->bIsPaused = true;

		if (Settings->Scope == EPCGExEventScope::Owner)
		{
			Context->SourceComponent->GetWorld()->GetSubsystem<UPCGExSubSystem>()->AddListener(
				PCGEx::FPCGExEvent(EPCGExEventScope::Owner, Settings->Event, Context->SourceComponent->GetOwner()),
				EventCallback);
		}
		else
		{
			Context->SourceComponent->GetWorld()->GetSubsystem<UPCGExSubSystem>()->AddListener(
				PCGEx::FPCGExEvent(EPCGExEventScope::Global, Settings->Event),
				EventCallback);
		}
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
