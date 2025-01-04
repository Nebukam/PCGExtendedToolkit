// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "Components/ActorComponent.h"
#include "PCGExSharedDataComponent.generated.h"


UCLASS(Hidden, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PCGEXTENDEDTOOLKIT_API UPCGExSharedDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPCGExSharedDataComponent();

	UPROPERTY()
	TSoftObjectPtr<UPCGComponent> PCGComponentInstance;
	
	UPROPERTY()
	TMap<FName, FPCGDataCollection> Collections;

	// Note : PCG Get Shared Data should first verify the existence of a component associated with the SourcePCG component that calls it
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void SetPCGComponent(UPCGComponent* InPCGComponentInstance);
	void RegisterSharedCollection(FName Key, const FPCGDataCollection& InCollection);

protected:
	void OnPCGComponentInstanceSet();
};
