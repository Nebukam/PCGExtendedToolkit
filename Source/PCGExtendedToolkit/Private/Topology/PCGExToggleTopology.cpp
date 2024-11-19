// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExToggleTopology.h"

#include "PCGGraph.h"
#include "PCGPin.h"
#include "Topology/PCGExDynamicMeshComponent.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE ToggleTopology

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExToggleTopologySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGEx::SourcePointsLabel, EPCGDataType::Any, true, true);

#if WITH_EDITOR
	PinPropertySource.Tooltip = FTEXT("In.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExToggleTopologySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(PCGEx::OutputPointsLabel, EPCGDataType::Any);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = FTEXT("Out.");
#endif

	return PinProperties;
}

FPCGElementPtr UPCGExToggleTopologySettings::CreateElement() const { return MakeShared<FPCGExToggleTopologyElement>(); }

#pragma endregion

FPCGContext* FPCGExToggleTopologyElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExToggleTopologyContext* Context = new FPCGExToggleTopologyContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExToggleTopologyElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ToggleTopology)

	if (AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr))
	{
		TArray<UPCGExDynamicMeshComponent*> Components;
		TargetActor->GetComponents<UPCGExDynamicMeshComponent>(Components);

		for (UPCGExDynamicMeshComponent* Component : Components)
		{
			if (!Component) { continue; }
			if (Settings->bFilterByTag && !Component->ComponentHasTag(Settings->FilterByTag)) { continue; }
			if (Settings->bToggle) { if (!Component->IsRegistered()) { Component->RegisterComponent(); } }
			else { if (Component->IsRegistered()) { Component->UnregisterComponent(); } }
		}
	}
	else
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid Target actor"));
	}

	DisabledPassThroughData(Context);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
