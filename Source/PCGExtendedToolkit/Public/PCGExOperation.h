// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExOperation.generated.h"

#define PCGEX_OVERRIDE_OPERATION_PROPERTY(_ACCESSOR, _NAME) { using T = decltype(_ACCESSOR); T OutValue = T{}; if(GetOverrideValue<T>(FName(TEXT(_NAME)), OutValue)){ _ACCESSOR = OutValue; }}
#define PCGEX_OVERRIDE_OPERATION_PROPERTY_SELECTOR(_ACCESSOR, _NAME)\
{\
	FString OutValue = TEXT("");\
	if (GetOverrideValue<FString>(FName(TEXT(_NAME)), OutValue))\
	{\
		FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();\
		NewSelector.Update(OutValue);\
		_ACCESSOR = NewSelector;\
	}\
}

namespace PCGExMT
{
	class FTaskManager;
}

class FPCGMetadataAttributeBase;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOperation : public UObject, public IPCGExManagedObjectInterface
{
	GENERATED_BODY()
	//~Begin UPCGExOperation interface
public:
	void BindContext(FPCGExContext* InContext);
	void FindSettingsOverrides(FPCGExContext* InContext);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void Cleanup() override;
	virtual void CopySettingsFrom(const UPCGExOperation* Other);

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

	virtual void ApplyOverrides();

	template <typename T>
	bool GetOverrideValue(const FName Name, T& OutValue)
	{
		FPCGMetadataAttributeBase** Att = PossibleOverrides.Find(Name);
		if (!Att || (*Att)->GetTypeId() != static_cast<int16>(PCGEx::GetMetadataType<T>())) { return false; }
		FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(*Att);
		OutValue = TypedAttribute->GetValue(PCGDefaultValueKey);
		return true;
	}

	//~End UPCGExOperation interface
};
