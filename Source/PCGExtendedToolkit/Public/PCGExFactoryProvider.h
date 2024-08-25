// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExMacros.h"
#include "PCGExGlobalSettings.h"
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
		StateNode,
		StateSocket,
		Sampler,
		Heuristics,
		VtxProperty,
		ConditionalActions
	};

	static inline TSet<EType> AnyFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterEdge, EType::FilterGroup};
	static inline TSet<EType> PointFilters = {EType::FilterPoint, EType::FilterGroup};
	static inline TSet<EType> ClusterNodeFilters = {EType::FilterPoint, EType::FilterNode, EType::FilterGroup};
	static inline TSet<EType> ClusterEdgeFilters = {EType::FilterPoint, EType::FilterEdge, EType::FilterGroup};
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
	virtual PCGExFactories::EType GetFactoryType() const { return PCGExFactories::EType::None; }
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
		FactoryProvider, "Factory : Proviader", "Creates an abstract factory provider.",
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
	virtual FName GetMainOutputLabel() const { return TEXT(""); }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory = nullptr) const;

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
	//~End UPCGExFactoryProviderSettings
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
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};

namespace PCGExFactories
{
	template <typename T_DEF>
	static bool GetInputFactories(const FPCGContext* InContext, const FName InLabel, TArray<T_DEF*>& OutFactories, const TSet<EType>& Types, const bool bThrowError = true)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);
		TSet<uint64> UniqueData;
		UniqueData.Reserve(Inputs.Num());

		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			bool bIsAlreadyInSet;
			UniqueData.Add(TaggedData.Data->UID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; }

			if (const T_DEF* State = Cast<T_DEF>(TaggedData.Data))
			{
				if (!Types.Contains(State->GetFactoryType()))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input '{0}' is not supported."), FText::FromString(State->GetClass()->GetName())));
					continue;
				}

				OutFactories.AddUnique(const_cast<T_DEF*>(State));
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
