// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExDynamicMeshComponent.h"

void UPCGExDynamicMeshComponent::SetManagedComponent(UPCGManagedComponent* InManagedComponent)
{
	ManagedComponent = InManagedComponent;
}

UPCGManagedComponent* UPCGExDynamicMeshComponent::GetManagedComponent()
{
	return ManagedComponent;
}
