// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExMacros.h"
#include "PCGExGlobalSettings.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGPointData.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.generated.h"

#define PCGEX_FACTORY_NAME_PRIORITY FName(FString::Printf(TEXT("(%02d) "), Priority) +  GetDisplayName())


///

namespace PCGExFactories
{
	enum class EType : uint8
	{
		None = 0,
		FilterGroup,
		FilterPoint,
		FilterNode,
		FilterEdge,
		RuleSort,
		RulePartition,
		Probe,
		NodeState,
		Sampler,
		Heuristics,
		VtxProperty,
		ConditionalActions,
		ShapeBuilder
	};

	static inline TSet<EType> AnyFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterEdge, EType::FilterGroup};
	static inline TSet<EType> PointFilters = {EType::FilterPoint, EType::FilterGroup};
	static inline TSet<EType> ClusterNodeFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterGroup};
	static inline TSet<EType> ClusterEdgeFilters = {EType::FilterPoint, EType::FilterEdge, EType::FilterGroup};
	static inline TSet<EType> SupportsClusterFilters = {EType::FilterEdge, EType::FilterNode, EType::NodeState, EType::FilterGroup};
	static inline TSet<EType> ClusterOnlyFilters = {EType::FilterEdge, EType::FilterNode, EType::NodeState};
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExParamDataBase : public UPCGPointData
{
	GENERATED_BODY()

public:
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExParamFactoryBase : public UPCGExParamDataBase
{
	GENERATED_BODY()

public:
	int32 Priority = 0;
	bool bDoRegisterConsumableAttributes = false;
	virtual PCGExFactories::EType GetFactoryType() const { return PCGExFactories::EType::None; }

	virtual void RegisterConsumableAttributes(FPCGExContext* InContext) const
	{
	}

	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const
	{
	}
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFactoryProviderSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		FactoryProvider, "Factory : Provider", "Creates an abstract factory provider.",
		FName(GetDisplayName()))
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilter; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
public:
	virtual FName GetMainOutputPin() const { return TEXT(""); }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory = nullptr) const;

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
	//~End UPCGExFactoryProviderSettings

	/** Whether this factory can register consumable attributes or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable), AdvancedDisplay)
	bool bDoRegisterConsumableAttributes = false;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFactoryProviderElement final : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};

namespace PCGExFactories
{
	template <typename T_DEF>
	static bool GetInputFactories(FPCGExContext* InContext, const FName InLabel, TArray<TObjectPtr<const T_DEF>>& OutFactories, const TSet<EType>& Types, const bool bThrowError = true)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);
		TSet<uint32> UniqueData;
		UniqueData.Reserve(Inputs.Num());

		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			bool bIsAlreadyInSet;
			UniqueData.Add(TaggedData.Data->GetUniqueID(), &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; }

			if (const T_DEF* Factory = Cast<T_DEF>(TaggedData.Data))
			{
				if (!Types.Contains(Factory->GetFactoryType()))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input '{0}' is not supported."), FText::FromString(Factory->GetClass()->GetName())));
					continue;
				}

				OutFactories.AddUnique(Factory);
				Factory->RegisterAssetDependencies(InContext);

				if (Factory->bDoRegisterConsumableAttributes) { Factory->RegisterConsumableAttributes(InContext); }
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input '{0}' is not supported."), FText::FromString(TaggedData.Data->GetClass()->GetName())));
			}
		}

		if (OutFactories.IsEmpty())
		{
			if (bThrowError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Missing required '{0}' inputs."), FText::FromName(InLabel))); }
			return false;
		}

		OutFactories.Sort([](const T_DEF& A, const T_DEF& B) { return A.Priority < B.Priority; });

		return true;
	}
}
