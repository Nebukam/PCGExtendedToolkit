// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCoreMacros.h" // Boilerplate
#include "Containers/PCGExManagedObjectsInterfaces.h"

#include "PCGExInstancedFactory.generated.h"

struct FPCGExContext;

namespace PCGEx
{
	class FManagedObjects;
}

namespace PCGExData
{
	class FFacadePreloader;
	class FFacade;
}

namespace PCGExMT
{
	class FTaskManager;
}

#define PCGEX_FACTORY_NEW_OPERATION(_TYPE) TSharedPtr<FPCGEx##_TYPE> NewOperation = MakeShared<FPCGEx##_TYPE>();

class FPCGMetadataAttributeBase;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class PCGEXCORE_API UPCGExInstancedFactory : public UObject, public IPCGExManagedObjectInterface
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

	UPCGExInstancedFactory* CreateNewInstance(PCGEx::FManagedObjects* InManagedObjects) const;

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
