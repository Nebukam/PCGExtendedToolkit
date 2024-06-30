// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatchAndSetFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "PCGExMatchAndSetAttributes.generated.h"

namespace PCGExMatchAndSetAttribute
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExMatchAndSetAttributesOperation : public UPCGExMatchAndSetOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache) override;
	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point) override;
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point) override;

	virtual void Cleanup() override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchAndSetAttributesFactory : public UPCGExMatchAndSetFactoryBase
{
	GENERATED_BODY()

public:
	virtual UPCGExMatchAndSetOperation* CreateOperation() const override;
	virtual bool Boot(FPCGContext* InContext) override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|MatchAndSetAttributes")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchAndSetAttributesProviderSettings : public UPCGExMatchAndSetProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MatchAndSetAttributesAttribute, "MatchAndSet : Attributes", "Forward attributes based on the match result.")

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
