// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once

#include "CoreMinimal.h"
#include "PCGExHelpers.h"
#include "Components/DynamicMeshComponent.h"
#include "PCGExDynamicMeshComponent.generated.h"


UCLASS(Hidden, meta=(BlueprintSpawnableComponent), ClassGroup = Rendering, MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDynamicMeshComponent : public UDynamicMeshComponent, public IPCGExManagedComponentInterface
{
	GENERATED_BODY()

public:
	virtual UPCGManagedComponent* GetManagedComponent() override;
	virtual void SetManagedComponent(UPCGManagedComponent* InManagedComponent) override;

	UPROPERTY()
	UPCGManagedComponent* ManagedComponent = nullptr;
};
