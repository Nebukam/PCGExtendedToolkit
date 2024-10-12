// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExConditionalActionFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"


#include "PCGExConditionalActionAttributes.generated.h"

namespace PCGExConditionalActionAttribute
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

class UPCGExConditionalActionAttributesFactory;

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionAttributesOperation : public UPCGExConditionalActionOperation
{
	GENERATED_BODY()

public:
	UPCGExConditionalActionAttributesFactory* TypedFactory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionAttributesFactory : public UPCGExConditionalActionFactoryBase
{
	friend class UPCGExConditionalActionAttributesProviderSettings;

	GENERATED_BODY()

public:
	virtual UPCGExConditionalActionOperation* CreateOperation(FPCGExContext* InContext) const override;
	virtual bool Boot(FPCGContext* InContext) override;

protected:
	FPCGExAttributeGatherDetails SuccessAttributesFilter;
	FPCGExAttributeGatherDetails FailAttributesFilter;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ConditionalActionAttributes")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionAttributesProviderSettings : public UPCGExConditionalActionProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConditionalActionWriteAttributes, "Action : Write Attributes", "Forward attributes based on the match result.")
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
