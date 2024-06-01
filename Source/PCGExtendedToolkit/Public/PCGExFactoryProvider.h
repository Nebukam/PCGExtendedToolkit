// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExPointsProcessor.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.generated.h"

///

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExParamDataBase : public UPCGPointData
{
	GENERATED_BODY()

public:
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExParamFactoryBase : public UPCGExParamDataBase
{
	GENERATED_BODY()
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExFactoryProviderSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		FactoryProvider, "Factory : Proviader", "Creates an abstract factory provider.",
		FName(GetDisplayName()))
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorFilter; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory = nullptr) const;

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
};

class PCGEXTENDEDTOOLKIT_API FPCGExFactoryProviderElement : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};
