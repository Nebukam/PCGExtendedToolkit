// Copyright 2024 Timothé Lapetite and contributors
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
	PCGEX_PIN_ANY(PCGEx::SourcePointsLabel, "In. Not used for anything except ordering operations.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExToggleTopologySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::SourcePointsLabel, "Out. Forwards In.", Required, {})
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

		TSet<TSoftObjectPtr<AActor>> OutActorsToDelete;

		for (UPCGExDynamicMeshComponent* Component : Components)
		{
			if (!Component) { continue; }

			if (Settings->bFilterByTag && !Component->ComponentHasTag(Settings->CommaSeparatedTagFilters)) { continue; }

			if (Settings->Action == EPCGExToggleTopologyAction::Remove)
			{
				if (Component->GetManagedComponent()) { Component->GetManagedComponent()->Release(true, OutActorsToDelete); }
			}
			else
			{
				if (Settings->bToggle) { if (!Component->IsRegistered()) { Component->RegisterComponent(); } }
				else { if (Component->IsRegistered()) { Component->UnregisterComponent(); } }
			}
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
