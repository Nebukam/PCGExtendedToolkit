// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Components/PCGExDynamicMeshComponent.h"

void UPCGExDynamicMeshComponent::SetManagedComponent(UPCGManagedComponent* InManagedComponent)
{
	ManagedComponent = InManagedComponent;
}

UPCGManagedComponent* UPCGExDynamicMeshComponent::GetManagedComponent()
{
	return ManagedComponent;
}
