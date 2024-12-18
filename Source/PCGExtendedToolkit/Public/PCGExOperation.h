// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBroadcast.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExOperation.generated.h"

namespace PCGExMT
{
	class FTaskManager;
}

class FPCGMetadataAttributeBase;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOperation : public UObject, public IPCGExManagedObjectInterface
{
	GENERATED_BODY()
	//~Begin UPCGExOperation interface
public:
	void BindContext(FPCGExContext* InContext);
	void FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void Cleanup() override;
	virtual void CopySettingsFrom(const UPCGExOperation* Other);

	virtual void RegisterAssetDependencies(FPCGExContext* InContext);
	
	TSharedPtr<PCGExData::FFacade> PrimaryDataFacade;
	TSharedPtr<PCGExData::FFacade> SecondaryDataFacade;

	template <typename T>
	T* CopyOperation() const
	{
		T* TypedInstance = Context->ManagedObjects->New<T>(GetTransientPackage(), this->GetClass());

		check(TypedInstance)

		TypedInstance->CopySettingsFrom(this);
		return TypedInstance;
	}

	virtual void BeginDestroy() override;

protected:
	FPCGExContext* Context = nullptr;
	TMap<FName, FPCGMetadataAttributeBase*> PossibleOverrides;

	void ApplyOverrides();

	//~End UPCGExOperation interface
};
