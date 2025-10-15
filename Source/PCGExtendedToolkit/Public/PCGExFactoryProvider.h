﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGSettings.h"
#include "UObject/Object.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExMT.h"
#include "Data/PCGExPointData.h"

#include "PCGExFactoryProvider.generated.h"

#define PCGEX_FACTORY_NAME_PRIORITY FName(FString::Printf(TEXT("(%d) "), Priority) +  GetDisplayName())
#define PCGEX_FACTORY_NEW_OPERATION(_TYPE) TSharedPtr<FPCGEx##_TYPE> NewOperation = MakeShared<FPCGEx##_TYPE>();
#define PCGEX_FACTORY_TYPE_ID(_TYPE)

///

namespace PCGExData
{
	class FFacade;
	class FFacadePreloader;
}

struct FPCGExFactoryProviderContext;

#define PCG_DECLARE_TYPE_INFO(...)
#define PCG_ASSIGN_TYPE_INFO(...)
#define PCG_DEFINE_TYPE_INFO(...)

USTRUCT()
struct FPCGDataTypeInfo
{
	GENERATED_BODY()
	
#if WITH_EDITOR
	virtual bool Hidden() const { return false; }
#endif // WITH_EDITOR
};

USTRUCT()
struct FPCGDataTypeInfoPoint : public FPCGDataTypeInfo
{
	GENERATED_BODY()
};

namespace PCGExFactories
{
	enum class EType : uint8
	{
		None = 0,
		Instanced,
		Filter,
		FilterGroup,
		FilterPoint,
		FilterNode,
		FilterEdge,
		FilterCollection,
		RuleSort,
		RulePartition,
		Probe,
		NodeState,
		Sampler,
		Heuristics,
		VtxProperty,
		Action,
		ShapeBuilder,
		Blending,
		TexParam,
		Tensor,
		IndexPicker,
		FillControls,
		MatchRule,
	};

	enum class EPreparationResult : uint8
	{
		None = 0,
		Success,
		MissingData,
		Fail
	};

	static inline TSet<EType> AnyFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterEdge, EType::FilterGroup, EType::FilterCollection};
	static inline TSet<EType> PointFilters = {EType::FilterPoint, EType::FilterGroup, EType::FilterCollection};
	static inline TSet<EType> ClusterNodeFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterGroup, EType::FilterCollection};
	static inline TSet<EType> ClusterEdgeFilters = {EType::FilterPoint, EType::FilterEdge, EType::FilterGroup, EType::FilterCollection};
	static inline TSet<EType> SupportsClusterFilters = {EType::FilterEdge, EType::FilterNode, EType::NodeState, EType::FilterGroup, EType::FilterCollection};
	static inline TSet<EType> ClusterOnlyFilters = {EType::FilterEdge, EType::FilterNode, EType::NodeState};
}


USTRUCT(DisplayName="PCGEx Subnode")
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
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) { return PCGExFactories::EPreparationResult::Success; }

	virtual void AddDataDependency(const UPCGData* InData);
	virtual void BeginDestroy() override;

protected:
	UPROPERTY()
	TSet<TObjectPtr<UPCGData>> DataDependencies;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExFactoryProviderSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExFactoryProviderElement;

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
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const override;
#endif

protected:
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

	/** A dummy property used to drive cache invalidation on settings changes */
	UPROPERTY()
	int32 InternalCacheInvalidator = 0;

	/** Cache the results of this node. Can yield unexpected result in certain cases.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	EPCGExOptionState CachingBehavior = EPCGExOptionState::Default;

	/** Whether this factory can register consumable attributes or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cleanup", meta = (PCG_NotOverridable))
	bool bCleanupConsumableAttributes = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietInvalidInputWarning = false;
	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietMissingAttributeError = false;
	
	/** If enabled, will turn off missing input errors on factories that have inputs with missing or no data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietMissingInputError = false;

#if WITH_EDITOR
	/** Open a browser and navigate to that node' documentation page. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Node Documentation", ShortToolTip="Open a browser and navigate to that node' documentation page", DisplayOrder=-1))
	void EDITOR_OpenNodeDocumentation() const;
#endif

protected:
	virtual bool ShouldCache() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFactoryProviderContext : FPCGExContext
{
	friend class FPCGExFactoryProviderElement;

	virtual ~FPCGExFactoryProviderContext() override;

	UPCGExFactoryData* OutFactory = nullptr;

	void LaunchDeferredCallback(PCGExMT::FSimpleCallback&& InCallback);

protected:
	TArray<TSharedPtr<PCGExMT::FDeferredCallbackHandle>> DeferredTasks;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFactoryProviderElement final : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual FPCGContext* CreateContext() override;
	
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;
};

namespace PCGExFactories
{
	bool GetInputFactories_Internal(FPCGExContext* InContext, const FName InLabel, TArray<TObjectPtr<const UPCGExFactoryData>>& OutFactories, const TSet<EType>& Types, const bool bRequired);

	template <typename T_DEF>
	static bool GetInputFactories(FPCGExContext* InContext, const FName InLabel, TArray<TObjectPtr<const T_DEF>>& OutFactories, const TSet<EType>& Types, const bool bRequired = true)
	{
		TArray<TObjectPtr<const UPCGExFactoryData>> BaseFactories;
		if (!GetInputFactories_Internal(InContext, InLabel, BaseFactories, Types, bRequired)) { return false; }

		// Cast back to T_DEF
		for (const TObjectPtr<const UPCGExFactoryData>& Base : BaseFactories) { if (const T_DEF* Derived = Cast<T_DEF>(Base)) { OutFactories.Add(Derived); } }

		return !OutFactories.IsEmpty();
	}

	PCGEXTENDEDTOOLKIT_API
	void RegisterConsumableAttributesWithData_Internal(const TArray<TObjectPtr<const UPCGExFactoryData>>& InFactories, FPCGExContext* InContext, const UPCGData* InData);

	PCGEXTENDEDTOOLKIT_API
	void RegisterConsumableAttributesWithFacade_Internal(const TArray<TObjectPtr<const UPCGExFactoryData>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InFacade);

	template <typename T_DEF>
	static void RegisterConsumableAttributesWithData(const TArray<TObjectPtr<const T_DEF>>& InFactories, FPCGExContext* InContext, const UPCGData* InData)
	{
		TArray<TObjectPtr<const UPCGExFactoryData>> BaseFactories;
		BaseFactories.Reserve(InFactories.Num());
		for (const TObjectPtr<const T_DEF>& Factory : InFactories) { BaseFactories.Add(Factory); }
		RegisterConsumableAttributesWithData_Internal(BaseFactories, InContext, InData);
	}

	template <typename T_DEF>
	static void RegisterConsumableAttributesWithFacade(const TArray<TObjectPtr<const T_DEF>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InFacade)
	{
		TArray<TObjectPtr<const UPCGExFactoryData>> BaseFactories;
		BaseFactories.Reserve(InFactories.Num());
		for (const TObjectPtr<const T_DEF>& Factory : InFactories) { BaseFactories.Add(Factory); }
		RegisterConsumableAttributesWithFacade_Internal(BaseFactories, InFacade);
	}
}
