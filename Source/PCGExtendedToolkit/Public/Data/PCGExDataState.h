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

		explicit TDataState(const UPCGExDataStateFactoryBase* InDefinition)
			: TFilter(InDefinition), StateDefinition(InDefinition)
		{
		}

		const UPCGExDataStateFactoryBase* StateDefinition;

		virtual bool Test(const int32 PointIndex) const override;
		virtual void PrepareForWriting(PCGExData::FPointIO* PointIO);

		virtual ~TDataState() override
		{
			OverlappingAttributes.Empty();

			InValidStateAttributes.Empty();
			InInvalidStateAttributes.Empty();

			OutValidStateAttributes.Empty();
			OutInvalidStateAttributes.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TStatesManager : public PCGExDataFilter::TFilterManager
	{
	public:
		TArray<int32> HighestState;
		bool bHasPartials = false;

		explicit TStatesManager(PCGExData::FPointIO* InPointIO)
			: TFilterManager(InPointIO)
		{
		}

		virtual void PrepareForTesting() override;
		virtual void PrepareForTesting(const TArrayView<int32>& PointIndices) override;

		virtual void Test(const int32 PointIndex) override;

		void WriteStateNames(FName AttributeName, FName DefaultValue, const TArray<int32>& InIndices);
		void WriteStateValues(FName AttributeName, int32 DefaultValue, const TArray<int32>& InIndices);
		void WriteStateIndividualStates(FPCGExAsyncManager* AsyncManager, const TArray<int32>& InIndices);

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
	static bool GetInputStates(FPCGContext* InContext, const FName InLabel, TArray<TObjectPtr<T_DEF>>& OutStates, const bool bAllowDuplicateNames)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& InputState : Inputs)
		{
			if (const T_DEF* State = Cast<T_DEF>(InputState.Data))
			{
				if (UniqueStatesNames.Contains(State->StateName) && !bAllowDuplicateNames)
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' has the same name as another state, it will be ignored."), FText::FromName(State->StateName)));
					continue;
				}

				if (State->Filters.IsEmpty())
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' has no conditions and will be ignored."), FText::FromName(State->StateName)));
					continue;
				}

				UniqueStatesNames.Add(State->StateName);
				OutStates.Add(const_cast<T_DEF*>(State));
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
	class PCGEXTENDEDTOOLKIT_API FWriteIndividualState : public FPCGExNonAbandonableTask
	{
	public:
		FWriteIndividualState(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			PCGExDataState::TDataState* InHandler, const TArray<int32>* InInIndices) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Handler(InHandler), InIndices(InInIndices)
		{
		}

		PCGExDataState::TDataState* Handler = nullptr;
		const TArray<int32>* InIndices = nullptr;

		virtual bool ExecuteTask() override;
	};
};
