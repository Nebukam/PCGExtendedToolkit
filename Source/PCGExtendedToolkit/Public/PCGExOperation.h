// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExOperation.generated.h"

#define PCGEX_OVERRIDE_OP_PROPERTY(_ACCESSOR, _NAME, _TYPE) _ACCESSOR = this->GetOverrideValue(_NAME, _ACCESSOR, _TYPE);

namespace PCGExMT
{
	class FTaskManager;
}

class FPCGMetadataAttributeBase;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class PCGEXTENDEDTOOLKIT_API UPCGExOperation : public UObject
{
	GENERATED_BODY()
	//~Begin UPCGExOperation interface
public:
	void BindContext(FPCGContext* InContext);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void Cleanup();
	virtual void CopySettingsFrom(const UPCGExOperation* Other);

	PCGExData::FFacade* PrimaryDataFacade = nullptr;
	PCGExData::FFacade* SecondaryDataFacade = nullptr;

	template <typename T>
	T* CopyOperation() const
	{
		PCGEX_NEW_FROM(UObject, GenericInstance, this)

		T* TypedInstance = Cast<T>(GenericInstance);

		if (!TypedInstance)
		{
			UPCGExOperation* Operation = Cast<UPCGExOperation>(GenericInstance);
			if (Operation) { PCGEX_DELETE_OPERATION(Operation) }
			else { PCGEX_DELETE_UOBJECT(GenericInstance) }
			return nullptr;
		}

		TypedInstance->CopySettingsFrom(this);
		return TypedInstance;
	}

	virtual void BeginDestroy() override;

protected:
	FPCGContext* Context = nullptr;
	TMap<FName, FPCGMetadataAttributeBase*> PossibleOverrides;

	virtual void ApplyOverrides();

	template <typename T>
	T GetOverrideValue(const FName Name, const T Fallback, const EPCGMetadataTypes InType)
	{
		FPCGMetadataAttributeBase** Att = PossibleOverrides.Find(Name);
		if (!Att || (*Att)->GetTypeId() != static_cast<int16>(InType)) { return Fallback; }
		FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(*Att);
		return TypedAttribute->GetValue(PCGInvalidEntryKey);
	}

	//~End UPCGExOperation interface
};
