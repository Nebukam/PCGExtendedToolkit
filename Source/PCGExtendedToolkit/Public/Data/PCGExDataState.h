// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Blending/PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExData.h"

#include "PCGExDataState.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExStateDefinitionBase : public UPCGExParamDataBase
{
	GENERATED_BODY()

public:
	FName StateName = NAME_None;
	int32 StateId = 0;
	int32 Priority = 0;

	TArray<TObjectPtr<UPCGParamData>> IfAttributes;
	TArray<PCGEx::FAttributesInfos*> IfInfos;

	TArray<TObjectPtr<UPCGParamData>> ElseAttributes;
	TArray<PCGEx::FAttributesInfos*> ElseInfos;

	virtual void BeginDestroy() override;
};

namespace PCGExDataState
{
	class PCGEXTENDEDTOOLKIT_API AStateHandler
	{
	public:
		explicit AStateHandler(UPCGExStateDefinitionBase* InDefinition):
			ADefinition(InDefinition)
		{
		}

		TArray<FPCGMetadataAttributeBase*> InIfAttributes;
		TArray<FPCGMetadataAttributeBase*> InElseAttributes;

		TArray<FPCGMetadataAttributeBase*> OutIfAttributes;
		TArray<FPCGMetadataAttributeBase*> OutElseAttributes;

		bool bValid = true;
		bool bPartial = false;
		int32 Index = 0;

		TSet<FString> OverlappingAttributes;
		UPCGExStateDefinitionBase* ADefinition = nullptr;

		TArray<bool> Results;

		virtual bool Test(const int32 PointIndex) const;

		virtual void PrepareForTesting(PCGExData::FPointIO* PointIO);
		virtual void PrepareForWriting(PCGExData::FPointIO* PointIO);

		virtual ~AStateHandler()
		{
			Results.Empty();
			OverlappingAttributes.Empty();

			InIfAttributes.Empty();
			InElseAttributes.Empty();

			OutIfAttributes.Empty();
			OutElseAttributes.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API AStatesManager
	{
	public:
		AStatesManager(PCGExData::FPointIO* InPointIO);

		TArray<AStateHandler*> Handlers;
		TArray<int32> HighestState;
		bool bValid = false;
		bool bHasPartials = false;

		PCGExData::FPointIO* PointIO = nullptr;

		template <typename T_DEF, typename T_HANDLER, class CaptureFunc>
		void Register(TArray<TObjectPtr<T_DEF>> Definitions, CaptureFunc&& Capture)
		{
			for (T_DEF* Def : Definitions)
			{
				T_HANDLER* Handler = new T_HANDLER(Def);
				Capture(Handler);

				if (!Handler->bValid)
				{
					delete Handler;
					continue;
				}

				Handlers.Add(Handler);
			}

			bValid = !Handlers.IsEmpty();

			if (!bValid) { return; }

			// Sort mappings so higher priorities come last, as they have to potential to override values.
			Handlers.Sort([&](const AStateHandler& A, const AStateHandler& B) { return A.ADefinition->Priority < B.ADefinition->Priority; });

			// Update index & partials
			bHasPartials = false;
			for (int i = 0; i < Handlers.Num(); i++)
			{
				Handlers[i]->Index = i;
				if (Handlers[i]->bPartial) { bHasPartials = true; }
			}
		}

		void PrepareForTesting();

		void Test(const int32 PointIndex);

		void WriteStateNames(FName AttributeName, FName DefaultValue);
		void WriteStateValues(FName AttributeName, int32 DefaultValue);
		void WriteStateIndividualStates(FPCGExAsyncManager* AsyncManager);
		
		void WritePrepareForStateAttributes(const FPCGContext* InContext);
		void WriteStateAttributes(FPCGExAsyncManager* AsyncManager);
		void WriteStateAttributes(const int32 PointIndex);

		~AStatesManager()
		{
			PCGEX_DELETE_TARRAY(Handlers)
		}

	protected:
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
	class PCGEXTENDEDTOOLKIT_API FWriteStateAttribute : public FPCGExNonAbandonableTask
	{
	public:
		FWriteStateAttribute(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			PCGExDataState::AStatesManager* InStateManager) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			StateManager(InStateManager)
		{
		}

		PCGExDataState::AStatesManager* StateManager = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FWriteIndividualState : public FPCGExNonAbandonableTask
	{
	public:
		FWriteIndividualState(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			PCGExDataState::AStateHandler* InHandler) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Handler(InHandler)
		{
		}

		PCGExDataState::AStateHandler* Handler = nullptr;

		virtual bool ExecuteTask() override;
	};
};
