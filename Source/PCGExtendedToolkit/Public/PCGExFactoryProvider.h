// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGSettings.h"
#include "UObject/Object.h"

#include "PCGExContext.h"
#include "PCGExElement.h"
#include "PCGExFactories.h"
#include "PCGExGlobalSettings.h"
#include "PCGExSettings.h"
#include "Data/PCGExPointData.h"
#include "PCGExVersion.h"

#include "PCGExFactoryProvider.generated.h"

#define PCGEX_FACTORY_NAME_PRIORITY FName(FString::Printf(TEXT("(%d) "), Priority) +  GetDisplayName())
#define PCGEX_FACTORY_NEW_OPERATION(_TYPE) TSharedPtr<FPCGEx##_TYPE> NewOperation = MakeShared<FPCGEx##_TYPE>();
#if PCGEX_ENGINE_VERSION > 506
#define PCGEX_FACTORY_TYPE_ID(_TYPE) virtual const FPCGDataTypeBaseId& GetFactoryTypeId() const{ return _TYPE::AsId(); }
#else
#define PCGEX_FACTORY_TYPE_ID(_TYPE)
#endif

///

namespace PCGExData
{
	class FFacade;
	class FFacadePreloader;
}

struct FPCGExFactoryProviderContext;

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Factory"))
struct FPCGExFactoryDataTypeInfo : public FPCGDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExParamDataBase : public UPCGExPointData
{
	GENERATED_BODY()

public:
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; } //PointOrParam would be best but it's gray and I don't like it

	virtual void OutputConfigToMetadata();
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFactoryData : public UPCGExParamDataBase
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExFactoryDataTypeInfo)

	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	bool bCleanupConsumableAttributes = false;

	PCGExFactories::EPreparationResult PrepResult = PCGExFactories::EPreparationResult::None;

	virtual PCGExFactories::EType GetFactoryType() const { return PCGExFactories::EType::None; }

	virtual bool RegisterConsumableAttributes(FPCGExContext* InContext) const;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const;
	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;

	virtual bool WantsPreparation(FPCGExContext* InContext) { return false; }
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) { return PCGExFactories::EPreparationResult::Success; }

	virtual void AddDataDependency(const UPCGData* InData);
	virtual void BeginDestroy() override;

protected:
	UPROPERTY()
	TSet<TObjectPtr<UPCGData>> DataDependencies;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExFactoryProviderSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExFactoryProviderElement;

#if PCGEX_ENGINE_VERSION > 506
	virtual const FPCGDataTypeBaseId& GetFactoryTypeId() const;
#endif

public:
	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	//PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FactoryProvider, "Factory : Provider", "Creates an abstract factory provider.", FName(GetDisplayName()))
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorDebug; }
#endif

	virtual int32 GetDefaultPriority() const { return 0; }

protected:
	UPROPERTY()
	int32 InternalCacheInvalidator = 0;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
public:
	virtual FName GetMainOutputPin() const { return TEXT(""); }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory = nullptr) const;
	virtual bool ShouldCancel(FPCGExFactoryProviderContext* InContext, PCGExFactories::EPreparationResult InResult) const { return true; }

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
	//~End UPCGExFactoryProviderSettings
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFactoryProviderContext : FPCGExContext
{
	friend class FPCGExFactoryProviderElement;
	UPCGExFactoryData* OutFactory = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFactoryProviderElement final : public IPCGExElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

public:
	PCGEX_ELEMENT_CREATE_CONTEXT(FactoryProvider)
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;
};