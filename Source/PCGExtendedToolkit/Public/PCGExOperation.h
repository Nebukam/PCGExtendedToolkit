// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExAttributeHelpers.h"
#include "UObject/Object.h"
#include "PCGExOperation.generated.h"

#define PCGEX_OVERRIDE_OP_PROPERTY(_ACCESSOR, _NAME, _TYPE) _ACCESSOR = this->GetOverrideValue(_NAME, _ACCESSOR, _TYPE);

class FPCGMetadataAttributeBase;
struct FPCGExPointsProcessorContext;
/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType, Blueprintable)
class PCGEXTENDEDTOOLKIT_API UPCGExOperation : public UObject
{
	GENERATED_BODY()
	//~Begin UPCGExOperation interface
public:
	void BindContext(FPCGExPointsProcessorContext* InContext);

#if WITH_EDITOR
	virtual void UpdateUserFacingInfos();
#endif

	virtual void Cleanup();
	virtual void Write();

	virtual void BeginDestroy() override;

protected:
	FPCGExPointsProcessorContext* Context = nullptr;
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
