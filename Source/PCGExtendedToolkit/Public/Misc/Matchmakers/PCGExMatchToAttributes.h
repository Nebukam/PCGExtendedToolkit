// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatchToFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "PCGExMatchToAttributes.generated.h"

namespace PCGExMatchToAttribute
{
	const FName SourceForwardSuccess = TEXT("MatchSuccess");
	const FName SourceForwardFail = TEXT("MatchFail");
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToAttributesOperation : public UPCGExMatchToOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point) override;
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point) override;

	virtual void Cleanup() override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToAttributesFactory : public UPCGExMatchToFactoryBase
{
	GENERATED_BODY()

public:
	virtual UPCGExMatchToOperation* CreateOperation() const override;
	virtual bool Boot(FPCGContext* InContext) override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|MatchToAttributes")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToAttributesProviderSettings : public UPCGExMatchToProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MatchToAttributesAttribute, "Match To : Attributes", "Forward attributes based on the match result.")
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
