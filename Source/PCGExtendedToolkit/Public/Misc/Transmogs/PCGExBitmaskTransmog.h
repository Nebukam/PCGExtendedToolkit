// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"
#include "PCGExOperation.h"

#include "PCGExBitmaskTransmog.generated.h"

#define PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(_NAME, _BODY) \
	UPCGExParamFactoryBase* UPCGEx##_NAME##ProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const{ \
	UPCGEx##_NAME##Factory* NewFactory = NewObject<UPCGEx##_NAME##Factory>(); _BODY Super::CreateFactory(InContext, NewFactory); return NewFactory; }

#define PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(_NAME, _BODY) \
UPCGExBitmaskTransmogOperation* UPCGEx##_NAME##Factory::CreateOperation() const{ \
	UPCGEx##_NAME##Operation* NewOperation = NewObject<UPCGEx##_NAME##Operation>(); \
	NewOperation->Factory = const_cast<UPCGEx##_NAME##Factory*>(this); _BODY \
	return NewOperation;}

namespace PCGExBitmaskTransmog
{
	const FName SourceTransmogsLabel = TEXT("Transmogs");
	const FName SourceDefaultsLabel = TEXT("Default values");
	const FName OutputTransmogLabel = TEXT("Transmog");
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskTransmogOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPCGExBitmaskTransmogFactoryBase* Factory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache);
	virtual void ProcessPoint(const FPCGPoint& Point, int64& Flags);

	virtual void Cleanup() override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskTransmogFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExAttributeGatherSettings InputAttributesFilter;

	PCGEx::FAttributesInfos* CheckSuccessInfos = nullptr;
	PCGEx::FAttributesInfos* CheckFailInfos = nullptr;

	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual UPCGExBitmaskTransmogOperation* CreateOperation() const;

	virtual bool Boot(FPCGContext* InContext);
	virtual bool AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage);

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|BitmaskTransmog")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskTransmogProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BitmaskTransmogAttribute, "Transmog : Abstract", "Abstract bitmask transmute settings.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
#endif
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for transmutation order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherSettings InputAttributesFilter;
};
