// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExToggleTopology.h"

#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Components/PCGExDynamicMeshComponent.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE ToggleTopology

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExToggleTopologySettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "In. Not used for anything except ordering operations.", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExToggleTopologySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "Out. Forwards In.", Required)
	return PinProperties;
}

FPCGElementPtr UPCGExToggleTopologySettings::CreateElement() const { return MakeShared<FPCGExToggleTopologyElement>(); }

#pragma endregion

bool FPCGExToggleTopologyElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
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
