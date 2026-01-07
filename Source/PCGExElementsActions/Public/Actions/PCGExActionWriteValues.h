// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExActionFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/Utils/PCGExDataFilterDetails.h"

#include "PCGExActionWriteValues.generated.h"

namespace PCGExData
{
	class IBuffer;
}

namespace PCGExActionWriteValues
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

class UPCGExActionWriteValuesFactory;

/**
 * 
 */
class FPCGExActionWriteValuesOperation : public FPCGExActionOperation
{
public:
	UPCGExActionWriteValuesFactory* TypedFactory = nullptr;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
	virtual void OnMatchSuccess(int32 Index) override;
	virtual void OnMatchFail(int32 Index) override;

protected:
	TArray<FPCGMetadataAttributeBase*> SuccessAttributes;
	TArray<TSharedPtr<PCGExData::IBuffer>> SuccessWriters;
	TArray<FPCGMetadataAttributeBase*> FailAttributes;
	TArray<TSharedPtr<PCGExData::IBuffer>> FailWriters;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExActionWriteValuesFactory : public UPCGExActionFactoryData
{
	friend class UPCGExActionWriteValuesProviderSettings;

	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExActionOperation> CreateOperation(FPCGExContext* InContext) const override;
	virtual bool Boot(FPCGContext* InContext) override;

protected:
	FPCGExAttributeGatherDetails SuccessAttributesFilter;
	FPCGExAttributeGatherDetails FailAttributesFilter;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ActionWriteValues", meta=(PCGExNodeLibraryDoc="quality-of-life/batch-actions/write-attributes"))
class UPCGExActionWriteValuesProviderSettings : public UPCGExActionProviderSettings
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
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails SuccessAttributesFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails FailAttributesFilter;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
