// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "PCGExFactoryProvider.h"

#include "PCGExInstancedFactory.generated.h"

namespace PCGExMT
{
	class FTaskManager;
}

class FPCGMetadataAttributeBase;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class PCGEXTENDEDTOOLKIT_API UPCGExInstancedFactory : public UObject, public IPCGExManagedObjectInterface
{
	GENERATED_BODY()
	//~Begin UPCGExInstancedOperation interface
public:
	void BindContext(FPCGExContext* InContext);
	virtual void InitializeInContext(FPCGExContext* InContext, FName InOverridesPinLabel);
	void FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void Cleanup() override;
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other);

	virtual bool WantsPerDataInstance() { return false; }

	virtual void RegisterAssetDependencies(FPCGExContext* InContext);

	TSharedPtr<PCGExData::FFacade> PrimaryDataFacade;
	TSharedPtr<PCGExData::FFacade> SecondaryDataFacade;

	template <typename T>
	T* CreateNewInstance(PCGEx::FManagedObjects* InManagedObjects) const
	{
		if (!InManagedObjects) { return nullptr; }
		T* TypedInstance = InManagedObjects->New<T>(GetTransientPackage(), this->GetClass());

		check(TypedInstance)

		TypedInstance->CopySettingsFrom(this);
		return TypedInstance;
	}

	virtual void RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const;
	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;

	virtual void BeginDestroy() override;
	virtual bool CanOnlyExecuteOnMainThread() const { return false; }

protected:
	FPCGExContext* Context = nullptr;
	TMap<FName, FPCGMetadataAttributeBase*> PossibleOverrides;

	void ApplyOverrides();

	//~End UPCGExInstancedOperation interface
};
