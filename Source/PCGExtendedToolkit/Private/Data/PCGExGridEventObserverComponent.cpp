// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExGridEventObserverComponent.h"


// Sets default values for this component's properties
UPCGExGridEventObserverComponent::UPCGExGridEventObserverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UPCGExGridEventObserverComponent::BeginPlay()
{
	Super::BeginPlay();
	// TODO : Register
}

void UPCGExGridEventObserverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// TODO : Unregister data associated with that component so it can be freed
}