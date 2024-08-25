// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExConditionalActionFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "PCGExOperation.h"

#include "PCGExConditionalActionDataTable.generated.h"

namespace PCGExConditionalActionDataTable
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

class UPCGExConditionalActionDataTableFactory;

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionDataTableOperation : public UPCGExConditionalActionOperation
{
	GENERATED_BODY()

public:
	UPCGExConditionalActionDataTableFactory* TypedFactory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point) override;
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point) override;

	virtual void Cleanup() override;

protected:
	TArray<FPCGMetadataAttributeBase*> SuccessAttributes;
	TArray<PCGEx::FAAttributeIO*> SuccessWriters;
	TArray<FPCGMetadataAttributeBase*> FailAttributes;
	TArray<PCGEx::FAAttributeIO*> FailWriters;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionDataTableFactory : public UPCGExConditionalActionFactoryBase
{
	friend class UPCGExConditionalActionDataTableProviderSettings;

	GENERATED_BODY()

public:
	virtual UPCGExConditionalActionOperation* CreateOperation() const override;
	virtual bool Boot(FPCGContext* InContext) override;

protected:
	TObjectPtr<UDataTable> DataTable;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ConditionalActionDataTable")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionDataTableProviderSettings : public UPCGExConditionalActionProviderSettings
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
	TSoftObjectPtr<UDataTable> DataTable;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
