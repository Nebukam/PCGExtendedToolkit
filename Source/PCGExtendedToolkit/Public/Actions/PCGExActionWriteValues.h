// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExActionFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"


#include "PCGExActionWriteValues.generated.h"

namespace PCGExActionWriteValues
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

class UPCGExActionWriteValuesFactory;

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionWriteValuesOperation : public UPCGExActionOperation
{
	GENERATED_BODY()

public:
	UPCGExActionWriteValuesFactory* TypedFactory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point) override;
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point) override;

	virtual void Cleanup() override;

protected:
	TArray<FPCGMetadataAttributeBase*> SuccessAttributes;
	TArray<TSharedPtr<PCGExData::FBufferBase>> SuccessWriters;
	TArray<FPCGMetadataAttributeBase*> FailAttributes;
	TArray<TSharedPtr<PCGExData::FBufferBase>> FailWriters;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionWriteValuesFactory : public UPCGExActionFactoryBase
{
	friend class UPCGExActionWriteValuesProviderSettings;

	GENERATED_BODY()

public:
	virtual UPCGExActionOperation* CreateOperation(FPCGExContext* InContext) const override;
	virtual bool Boot(FPCGContext* InContext) override;

protected:
	FPCGExAttributeGatherDetails SuccessAttributesFilter;
	FPCGExAttributeGatherDetails FailAttributesFilter;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ActionWriteValues")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionWriteValuesProviderSettings : public UPCGExActionProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ActionWriteAttributes, "Action : Write Attributes", "Forward attributes based on the match result.")
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails SuccessAttributesFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails FailAttributesFilter;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
