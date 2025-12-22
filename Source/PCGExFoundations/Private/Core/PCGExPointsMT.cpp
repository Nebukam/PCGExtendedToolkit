// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExPointsMT.h"


#include "Factories/PCGExInstancedFactory.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Core/PCGExPointFilter.h"

namespace PCGExPointsMT
{
#pragma region Tasks

	template <typename T>
	class FStartBatchProcessing final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FStartBatchProcessing)

		FStartBatchProcessing(TSharedPtr<T> InTarget)
			: FTask(), Target(InTarget)
		{
		}

		TSharedPtr<T> Target;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
		{
			Target->Process(TaskManager);
		}
	};

#pragma endregion

	IProcessor::IProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
	}

	void IProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		check(InContext)
		ExecutionContext = InContext;
		WorkHandle = ExecutionContext->GetWorkHandle();
	}

	void IProcessor::SetPointsFilterData(TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFactories)
	{
		FilterFactories = InFactories;
	}

	void IProcessor::RegisterConsumableAttributesWithFacade() const
	{
		// Gives an opportunity for the processor to register attributes with a valid facade
		// So selectors shortcut can be properly resolved (@Last, etc.)

		if (FilterFactories) { PCGExFactories::RegisterConsumableAttributesWithFacade(*FilterFactories, PointDataFacade); }
		if (PrimaryInstancedFactory) { PrimaryInstancedFactory->RegisterConsumableAttributesWithFacade(ExecutionContext, PointDataFacade); }
	}

	void IProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (HasFilters()) { PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *FilterFactories, FacadePreloader); }
	}

	void IProcessor::PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
	{
		TaskManager = InTaskManager;

		InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>(PointDataFacade);
		RegisterBuffersDependencies(*InternalFacadePreloader);

		InternalFacadePreloader->StartLoading(TaskManager, InPrefetchDataTaskGroup);
	}

	bool IProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TaskManager = InTaskManager;
		PCGEX_ASYNC_CHKD(TaskManager)

#pragma region Primary filters

		if (FilterFactories)
		{
			InitPrimaryFilters(FilterFactories);
		}

#pragma endregion

		if (PrimaryInstancedFactory)
		{
			if (PrimaryInstancedFactory->WantsPerDataInstance())
			{
				PrimaryInstancedFactory = PrimaryInstancedFactory->CreateNewInstance(ExecutionContext->ManagedObjects.Get());
				if (!PrimaryInstancedFactory) { return false; }
				PrimaryInstancedFactory->PrimaryDataFacade = PointDataFacade;
			}
		}

		return true;
	}

	void IProcessor::StartParallelLoopForPoints(const PCGExData::EIOSide Side, const int32 PerLoopIterations)
	{
		const UPCGBasePointData* CurrentProcessingSource = const_cast<UPCGBasePointData*>(PointDataFacade->GetData(Side));
		if (!CurrentProcessingSource) { return; }

		const int32 NumPoints = CurrentProcessingSource->GetNumPoints();

		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(Points, NumPoints, PrepareLoopScopesForPoints, ProcessPoints, OnPointsProcessingComplete, bForceSingleThreadedProcessPoints)
	}

	void IProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
	}

	void IProcessor::OnPointsProcessingComplete()
	{
	}

	void IProcessor::StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(Ranges, NumIterations, PrepareLoopScopesForRanges, ProcessRange, OnRangeProcessingComplete, bForceSingleThreadedProcessRange)
	}

	void IProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void IProcessor::OnRangeProcessingComplete()
	{
	}

	void IProcessor::CompleteWork()
	{
	}

	void IProcessor::Write()
	{
	}

	void IProcessor::Output()
	{
	}

	void IProcessor::Cleanup()
	{
		bIsProcessorValid = false;
	}

	bool IProcessor::InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories)
	{
		PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
	}

	int32 IProcessor::FilterScope(const PCGExMT::FScope& Scope, const bool bParallel)
	{
		if (PrimaryFilters) { return PrimaryFilters->Test(Scope, PointFilterCache, bParallel); }
		return DefaultPointFilterValue ? Scope.Count : 0;
	}

	int32 IProcessor::FilterAll()
	{
		return FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum()), true);
	}

	TSharedPtr<IProcessor> IBatch::NewProcessorInstance(const TSharedRef<PCGExData::FFacade>& InPointDataFacade) const
	{
		return nullptr;
	}

	IBatch::IBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: ExecutionContext(InContext), PointsCollection(InPointsCollection)
	{
		SetExecutionContext(InContext);
	}

	void IBatch::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkHandle = ExecutionContext->GetWorkHandle();
	}

	bool IBatch::PrepareProcessing()
	{
		return true;
	}

	void IBatch::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		if (PointsCollection.IsEmpty()) { return; }

		CurrentState.store(PCGExCommon::States::State_Processing, std::memory_order_release);

		TaskManager = InTaskManager;
		PCGEX_ASYNC_CHKD_VOID(TaskManager)

		TSharedPtr<IBatch> SelfPtr = SharedThis(this);

		const bool bDoInitData = DataInitializationPolicy == PCGExData::EIOInit::Duplicate || DataInitializationPolicy == PCGExData::EIOInit::New;

		for (const TWeakPtr<PCGExData::FPointIO>& WeakIO : PointsCollection)
		{
			TSharedPtr<PCGExData::FPointIO> IO = WeakIO.Pin();

			PCGEX_MAKE_SHARED(PointDataFacade, PCGExData::FFacade, IO.ToSharedRef())

			const TSharedRef<IProcessor> NewProcessor = NewProcessorInstance(PointDataFacade.ToSharedRef()).ToSharedRef();

			NewProcessor->SetExecutionContext(ExecutionContext);
			NewProcessor->ParentBatch = SharedThis(this);
			NewProcessor->BatchIndex = Processors.Num();

			if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
			if (PrimaryInstancedFactory) { NewProcessor->PrimaryInstancedFactory = PrimaryInstancedFactory; }

			NewProcessor->RegisterConsumableAttributesWithFacade();

			if (!PrepareSingle(NewProcessor)) { continue; }

			Processors.Add(NewProcessor);

			ProcessorFacades.Add(NewProcessor->PointDataFacade);
			SubProcessorMap->Add(&NewProcessor->PointDataFacade->Source.Get(), NewProcessor);

			NewProcessor->bIsTrivial = IO->GetNum() < PCGEX_CORE_SETTINGS.SmallPointsSize;

			if (bDoInitData) { NewProcessor->PointDataFacade->Source->InitializeOutput(DataInitializationPolicy); }
		}

		if (Processors.IsEmpty())
		{
			return;
		}

		if (bPrefetchData)
		{
			PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, ParallelAttributeRead)

			ParallelAttributeRead->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnProcessingPreparationComplete();
			};

			ParallelAttributeRead->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE, ParallelAttributeRead](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->Processors[Index]->PrefetchData(This->TaskManager, ParallelAttributeRead);
			};

			ParallelAttributeRead->StartIterations(Processors.Num(), 1);
		}
		else
		{
			OnProcessingPreparationComplete();
		}
	}

	void IBatch::OnInitialPostProcess()
	{
	}

	bool IBatch::PrepareSingle(const TSharedRef<IProcessor>& InProcessor)
	{
		return true;
	}

	void IBatch::CompleteWork()
	{
		if (bSkipCompletion) { return; }
		CurrentState.store(PCGExCommon::States::State_Completing, std::memory_order_release);
		PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bForceSingleThreadedCompletion, { Processor->CompleteWork(); }, {})
	}

	void IBatch::Write()
	{
		CurrentState.store(PCGExCommon::States::State_Writing, std::memory_order_release);
		PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bForceSingleThreadedWrite, { Processor->Write(); }, {})
	}

	void IBatch::Output()
	{
		for (const TSharedRef<IProcessor>& P : Processors)
		{
			if (!P->bIsProcessorValid) { continue; }
			P->Output();
		}
	}

	void IBatch::Cleanup()
	{
		ProcessorFacades.Empty();

		for (const TSharedRef<IProcessor>& P : Processors) { P->Cleanup(); }
		Processors.Empty();
	}

	void IBatch::OnProcessingPreparationComplete()
	{
		PCGEX_ASYNC_MT_LOOP_TPL(Process, bForceSingleThreadedProcessing, { Processor->bIsProcessorValid = Processor->Process(This->TaskManager); }, { Process->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE](){ PCGEX_ASYNC_THIS This->OnInitialPostProcess(); };})
	}

	void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<IBatch>& Batch)
	{
		PCGEX_LAUNCH(FStartBatchProcessing<IBatch>, Batch)
	}
}
