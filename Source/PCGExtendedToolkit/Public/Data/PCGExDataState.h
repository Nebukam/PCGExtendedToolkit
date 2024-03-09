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
class PCGEXTENDEDTOOLKIT_API UPCGExStateDefinitionBase : public UPCGExFilterDefinitionBase
{
	GENERATED_BODY()

public:
	FName StateName = NAME_None;
	int32 StateId = 0;

	TArray<TObjectPtr<UPCGParamData>> IfAttributes;
	TArray<PCGEx::FAttributesInfos*> IfInfos;

	TArray<TObjectPtr<UPCGParamData>> ElseAttributes;
	TArray<PCGEx::FAttributesInfos*> ElseInfos;

	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExDataState
{
	const FName OutputTestLabel = TEXT("Test");
	const FName SourceTestsLabel = TEXT("Tests");
	const FName SourceIfAttributesLabel = TEXT("If");
	const FName SourceElseAttributesLabel = TEXT("Else");

	class PCGEXTENDEDTOOLKIT_API TStateHandler : public PCGExDataFilter::TFilterHandler
	{
	public:
		TArray<FPCGMetadataAttributeBase*> InIfAttributes;
		TArray<FPCGMetadataAttributeBase*> InElseAttributes;

		TArray<FPCGMetadataAttributeBase*> OutIfAttributes;
		TArray<FPCGMetadataAttributeBase*> OutElseAttributes;

		bool bPartial = false;

		TSet<FString> OverlappingAttributes;

		explicit TStateHandler(const UPCGExStateDefinitionBase* InDefinition)
			: TFilterHandler(InDefinition), StateDefinition(InDefinition)
		{
		}

		const UPCGExStateDefinitionBase* StateDefinition;

		virtual bool Test(const int32 PointIndex) const override;
		virtual void PrepareForWriting(PCGExData::FPointIO* PointIO);

		virtual ~TStateHandler() override
		{
			OverlappingAttributes.Empty();

			InIfAttributes.Empty();
			InElseAttributes.Empty();

			OutIfAttributes.Empty();
			OutElseAttributes.Empty();
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

		virtual void Test(const int32 PointIndex) override;

		void WriteStateNames(FName AttributeName, FName DefaultValue);
		void WriteStateValues(FName AttributeName, int32 DefaultValue);
		void WriteStateIndividualStates(FPCGExAsyncManager* AsyncManager);

		void WritePrepareForStateAttributes(const FPCGContext* InContext);
		void WriteStateAttributes(const int32 PointIndex);

		virtual ~TStatesManager() override
		{
			PCGEX_DELETE_TARRAY(Handlers)
		}

	protected:
		virtual void PostProcessHandler(PCGExDataFilter::TFilterHandler* Handler) override;
	};

	template <typename T_DEF>
	static bool GetInputStates(FPCGContext* InContext, const FName InLabel, TArray<TObjectPtr<T_DEF>>& OutStates)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& InputState : Inputs)
		{
			if (const T_DEF* State = Cast<T_DEF>(InputState.Data))
			{
				if (UniqueStatesNames.Contains(State->StateName))
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("State '{0}' has the same name as another state, it will be ignored."), FText::FromName(State->StateName)));
					continue;
				}

				if (State->Tests.IsEmpty())
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
			PCGExDataState::TStateHandler* InHandler) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Handler(InHandler)
		{
		}

		PCGExDataState::TStateHandler* Handler = nullptr;

		virtual bool ExecuteTask() override;
	};
};
