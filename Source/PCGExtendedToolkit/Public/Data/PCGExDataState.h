// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Blending/PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExData.h"
#include "PCGExDataFilter.h"

#include "PCGExDataState.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExDataStateFactoryBase : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FName StateName = NAME_None;
	int32 StateId = 0;

	TArray<TObjectPtr<UPCGParamData>> ValidStateAttributes;
	TArray<PCGEx::FAttributesInfos*> ValidStateAttributesInfos;

	TArray<TObjectPtr<UPCGParamData>> InvalidStateAttributes;
	TArray<PCGEx::FAttributesInfos*> InvalidStateAttributesInfos;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExDataState
{
	const FName OutputFilterLabel = TEXT("Filter");
	const FName SourceFiltersLabel = TEXT("Filters");
	const FName SourceValidStateAttributesLabel = TEXT("ValidStateAttributes");
	const FName SourceInvalidStateAttributesLabel = TEXT("InvalidStateAttributes");

	class PCGEXTENDEDTOOLKIT_API TDataState : public PCGExDataFilter::TFilter
	{
	public:
		TArray<FPCGMetadataAttributeBase*> InValidStateAttributes;
		TArray<FPCGMetadataAttributeBase*> InInvalidStateAttributes;

		TArray<FPCGMetadataAttributeBase*> OutValidStateAttributes;
		TArray<FPCGMetadataAttributeBase*> OutInvalidStateAttributes;

		bool bPartial = false;

		TSet<FString> OverlappingAttributes;

		explicit TDataState(const UPCGExDataStateFactoryBase* InFactory)
			: TFilter(InFactory), StateFactory(InFactory)
		{
		}

		const UPCGExDataStateFactoryBase* StateFactory;

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;
		virtual void PrepareForWriting();

		virtual ~TDataState() override
		{
			OverlappingAttributes.Empty();

			InValidStateAttributes.Empty();
			InInvalidStateAttributes.Empty();

			OutValidStateAttributes.Empty();
			OutInvalidStateAttributes.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TStatesManager final : public PCGExDataFilter::TFilterManager
	{
	public:
		TArray<int32> HighestState;
		bool bHasPartials = false;

		PCGExDataCaching::FPool* EdgeDataCache = nullptr;

		explicit TStatesManager(PCGExDataCaching::FPool* InVtxDataCache, PCGExDataCaching::FPool* InEdgeDataCache)
			: TFilterManager(InVtxDataCache), EdgeDataCache(InEdgeDataCache)
		{
		}

		virtual void PrepareForTesting() override;
		virtual void PrepareForTesting(const TArrayView<const int32>& PointIndices) override;

		virtual void Test(const int32 PointIndex) override;

		void WriteStateNames(PCGExMT::FTaskManager* AsyncManager, FName AttributeName, FName DefaultValue, const TArrayView<const int32>& InIndices);
		void WriteStateValues(PCGExMT::FTaskManager* AsyncManager, FName AttributeName, int32 DefaultValue, const TArrayView<const int32>& InIndices);
		void WriteStateIndividualStates(PCGExMT::FTaskManager* AsyncManager, const TArrayView<const int32>& InIndices);

		void WritePrepareForStateAttributes(const FPCGContext* InContext);
		void WriteStateAttributes(const int32 PointIndex);

		virtual ~TStatesManager() override
		{
			PCGEX_DELETE_TARRAY(Handlers)
		}

	protected:
		virtual void PostProcessHandler(PCGExDataFilter::TFilter* Handler) override;
	};

	template <typename T_DEF>
	static bool GetInputStateFactories(FPCGContext* InContext, const FName InLabel, TArray<T_DEF*>& OutStates, const TSet<PCGExFactories::EType>& Types, const bool bAllowDuplicateNames)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			if (const T_DEF* State = Cast<T_DEF>(TaggedData.Data))
			{
				if (!Types.Contains(State->GetFactoryType()))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' is not supported."), FText::FromName(State->StateName)));
					continue;
				}

				if (UniqueStatesNames.Contains(State->StateName) && !bAllowDuplicateNames)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' has the same name as another state, it will be ignored."), FText::FromName(State->StateName)));
					continue;
				}

				if (State->FilterFactories.IsEmpty())
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' has no conditions and will be ignored."), FText::FromName(State->StateName)));
					continue;
				}

				UniqueStatesNames.Add(State->StateName);
				OutStates.Add(const_cast<T_DEF*>(State));
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' is not supported."), FText::FromString(TaggedData.Data->GetClass()->GetName())));
			}
		}

		UniqueStatesNames.Empty();

		if (OutStates.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing valid node states."));
			return false;
		}

		return true;
	}
}

namespace PCGExDataStateTask
{
	class PCGEXTENDEDTOOLKIT_API FWriteIndividualState final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteIndividualState(
			PCGExData::FPointIO* InPointIO,
			PCGExDataState::TDataState* InHandler,
			const TArrayView<const int32>& InIndices) :
			FPCGExTask(InPointIO),
			Handler(InHandler),
			Indices(InIndices)
		{
		}

		PCGExDataState::TDataState* Handler = nullptr;
		const TArrayView<const int32> Indices;

		virtual bool ExecuteTask() override;
	};
};
