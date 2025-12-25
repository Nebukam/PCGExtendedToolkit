// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"

#include "Components/DynamicMeshComponent.h"
#include "Containers/PCGExManagedObjectsInterfaces.h"
#include "PCGExDynamicMeshComponent.generated.h"


UCLASS(Hidden, meta=(BlueprintSpawnableComponent), ClassGroup = Rendering)
class PCGEXELEMENTSTOPOLOGY_API UPCGExDynamicMeshComponent : public UDynamicMeshComponent, public IPCGExManagedComponentInterface
{
	GENERATED_BODY()

public:
	virtual UPCGManagedComponent* GetManagedComponent() override;
	virtual void SetManagedComponent(UPCGManagedComponent* InManagedComponent) override;

	UPROPERTY()
	TObjectPtr<UPCGManagedComponent> ManagedComponent = nullptr;
};
