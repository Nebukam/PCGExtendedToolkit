// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

//#include "Elements/PCGSplineMeshParams.h"

//#include "Engine/SplineMeshComponentDescriptor.h"
#include "PCGComponent.h"
#include "PCGManagedResource.h"

#include "PCGExManagedResource.generated.h"

class UPCGComponent;

namespace PCGExPaths
{
	struct FSplineMeshSegment;
}

class UActorComponent;
class UInstancedStaticMeshComponent;
class USplineMeshComponent;

namespace PCGExManagedRessource
{
	template <typename T>
	static T* CreateResource(UPCGComponent* InSourceComponent, uint64 SettingsUID)
	{
		T* Resource = NewObject<T>(InSourceComponent);
		Resource->SetSettingsUID(SettingsUID);
		InSourceComponent->AddToManagedResources(Resource);
		return Resource;
	}
}

UCLASS(BlueprintType)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExManagedSplineMeshComponent : public UPCGManagedComponent
{
	GENERATED_BODY()

public:
	//~Begin UPCGManagedComponents interface
	virtual void ResetComponent() override
	{
		/* Does nothing, but implementation is required to support reuse. */
	}

	virtual bool SupportsComponentReset() const override { return false; } // true once we implement it properly
	virtual void ForgetComponent() override;
	//~End UPCGManagedComponents interface

	USplineMeshComponent* GetComponent() const;
	void SetComponent(USplineMeshComponent* InComponent);

	//void SetDescriptor(const FSplineMeshComponentDescriptor& InDescriptor) { Descriptor = InDescriptor; }
	//const FSplineMeshComponentDescriptor& GetDescriptor() const { return Descriptor; }

	//void SetSplineMeshParams(const FPCGSplineMeshParams& InSplineMeshParams) { SplineMeshParams = InSplineMeshParams; }
	//const FPCGSplineMeshParams& GetSplineMeshParams() const { return SplineMeshParams; }

	uint64 GetSettingsUID() const { return SettingsUID; }
	void SetSettingsUID(const uint64 InSettingsUID) { SettingsUID = InSettingsUID; }

	void AttachTo(AActor* InTargetActor, const UPCGComponent* InSourceComponent) const;

	static USplineMeshComponent* CreateComponentOnly(AActor* InOuter, UPCGComponent* InSourceComponent, const PCGExPaths::FSplineMeshSegment& InParams);
	static UPCGExManagedSplineMeshComponent* RegisterAndAttachComponent(AActor* InOuter, USplineMeshComponent* InSMC, UPCGComponent* InSourceComponent, uint64 SettingsUID);

	static UPCGExManagedSplineMeshComponent* GetOrCreate(AActor* InOuter, UPCGComponent* InSourceComponent, uint64 SettingsUID, const PCGExPaths::FSplineMeshSegment& InParams, const bool bForceNew = false);

protected:
	//UPROPERTY()
	//FSplineMeshComponentDescriptor Descriptor;

	//UPROPERTY()
	//FPCGSplineMeshParams SplineMeshParams;

	UPROPERTY(Transient)
	uint64 SettingsUID = -1; // purposefully a value that will never happen in data

	// Cached raw pointer to USplineMeshComponent
	mutable USplineMeshComponent* CachedRawComponentPtr = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
