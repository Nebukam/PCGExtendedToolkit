// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExEditorSettings.h"
#include "PCGExPointsProcessor.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.generated.h"

///

namespace PCGExFactories
{
	enum class EType : uint8
	{
		Default = 0,
		Filter,
		ClusterFilter,
		NodeState,
		SocketState,
		Sampler,
		Heuristics
	};

	static inline TSet<EType> ClusterFilters = {EType::Filter, EType::ClusterFilter};
}

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

public:
	FORCEINLINE virtual PCGExFactories::EType GetFactoryType() const;
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
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorFilter; }
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

namespace PCGExFactories
{
	template <typename T_DEF>
	static bool GetInputFactories(FPCGContext* InContext, const FName InLabel, TArray<T_DEF*>& OutFactories, const TSet<EType>& Types, const bool bThrowError = true)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& TaggedData : Inputs)
		{
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

		UniqueStatesNames.Empty();

		if (OutFactories.IsEmpty())
		{
			if (bThrowError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing valid filters.")); }
			return false;
		}

		return true;
	}
}
