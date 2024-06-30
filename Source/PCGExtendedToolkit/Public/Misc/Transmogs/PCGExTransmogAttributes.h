// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBitmaskTransmog.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "PCGExTransmogAttributes.generated.h"

namespace PCGExTransmogAttribute
{
	const FName SourceForwardSuccess = TEXT("ForwardOnSuccess");
	const FName SourceForwardFail = TEXT("ForwardOnFail");
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExTransmogAttributesOperation : public UPCGExBitmaskTransmogOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache);
	virtual void ProcessPoint(const FPCGPoint& Point, int64& Flags);

	virtual void Cleanup() override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExTransmogAttributesFactory : public UPCGExBitmaskTransmogFactoryBase
{
	GENERATED_BODY()

public:
	virtual UPCGExBitmaskTransmogOperation* CreateOperation() const override;
	virtual bool Boot(FPCGContext* InContext);
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|TransmogAttributes")
class PCGEXTENDEDTOOLKIT_API UPCGExTransmogAttributesProviderSettings : public UPCGExBitmaskTransmogProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TransmogAttributesAttribute, "Transmog : Attribute", "Forward attributes based on the check result.")

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
