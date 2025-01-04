// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExSharedDataComponent.h"


// Sets default values for this component's properties
UPCGExSharedDataComponent::UPCGExSharedDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UPCGExSharedDataComponent::BeginPlay()
{
	Super::BeginPlay();
	UPCGComponent* PCGComponent = PCGComponentInstance.Get();
	if (!PCGComponent) { return; }
}

void UPCGExSharedDataComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// TODO : Unregister data associated with that component so it can be freed
}

void UPCGExSharedDataComponent::SetPCGComponent(UPCGComponent* InPCGComponentInstance)
{
	if (!PCGComponentInstance)
	{
		PCGComponentInstance = InPCGComponentInstance;
		if (PCGComponentInstance) { OnPCGComponentInstanceSet(); }
		return;
	}

	// TODO : 
}

void UPCGExSharedDataComponent::RegisterSharedCollection(FName Key, const FPCGDataCollection& InCollection)
{
}

void UPCGExSharedDataComponent::OnPCGComponentInstanceSet()
{
	UPCGComponent* PCGComponent = PCGComponentInstance.Get();
	if (PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad)
	{
		// TODO : Dispatch data immediately?
		if (PCGComponent->IsGenerating())
		{
			// TODO : Hook to OnGenerated
		}
		else
		{
			// TODO : Check if we have data in cache
		}
	}
	else if (PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand)
	{
		// TODO : Hook to OnGenerated
	}
}
