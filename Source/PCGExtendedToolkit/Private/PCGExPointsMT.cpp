// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsMT.h"

#include "PCGExInstancedFactory.h"


namespace PCGExPointsMT
{
	IPointsProcessor::IPointsProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
		PCGEX_LOG_CTR(FPointsProcessor)
	}

	void IPointsProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		check(InContext)
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	void IPointsProcessor::SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFactories)
	{
		FilterFactories = InFactories;
	}

	void IPointsProcessor::RegisterConsumableAttributesWithFacade() const
	{
		// Gives an opportunity for the processor to register attributes with a valid facade
		// So selectors shortcut can be properly resolved (@Last, etc.)

		if (FilterFactories) { PCGExFactories::RegisterConsumableAttributesWithFacade(*FilterFactories, PointDataFacade); }
		if (PrimaryInstancedFactory) { PrimaryInstancedFactory->RegisterConsumableAttributesWithFacade(ExecutionContext, PointDataFacade); }
	}

	void IPointsProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (HasFilters()) { PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *FilterFactories, FacadePreloader); }
	}

	void IPointsProcessor::PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
	{
		AsyncManager = InAsyncManager;

		InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>(PointDataFacade);
		RegisterBuffersDependencies(*InternalFacadePreloader);

		InternalFacadePreloader->StartLoading(AsyncManager, InPrefetchDataTaskGroup);
	}

	bool IPointsProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		AsyncManager = InAsyncManager;
		PCGEX_ASYNC_CHKD(AsyncManager)

#pragma region Path filter data

		if (FilterFactories)
		{
			InitPrimaryFilters(FilterFactories);
			/*
			if(PrimaryFilters && !PrimaryFilters->Test(PointDataFacade->Source))
			{
				// Filtered out data
				// TODO : Check that this is not creating weird issues.
				return false;
			}
			*/
		}

#pragma endregion

		if (PrimaryInstancedFactory)
		{
			if (PrimaryInstancedFactory->WantsPerDataInstance())
			{
				PrimaryInstancedFactory = PrimaryInstancedFactory->CreateNewInstance<UPCGExInstancedFactory>(ExecutionContext->ManagedObjects.Get());
				if (!PrimaryInstancedFactory) { return false; }
				PrimaryInstancedFactory->PrimaryDataFacade = PointDataFacade;
			}
		}

		return true;
	}

	void IPointsProcessor::StartParallelLoopForPoints(const PCGExData::EIOSide Side, const int32 PerLoopIterations)
	{
		const UPCGBasePointData* CurrentProcessingSource = const_cast<UPCGBasePointData*>(PointDataFacade->GetData(Side));
		if (!CurrentProcessingSource) { return; }

		const int32 NumPoints = CurrentProcessingSource->GetNumPoints();

		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Points, NumPoints,
			PrepareLoopScopesForPoints, ProcessPoints,
			OnPointsProcessingComplete,
			bDaisyChainProcessPoints)
	}

	void IPointsProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IPointsProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
	}

	void IPointsProcessor::OnPointsProcessingComplete()
	{
	}

	void IPointsProcessor::StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Ranges, NumIterations,
			PrepareLoopScopesForRanges, ProcessRange,
			OnRangeProcessingComplete,
			bDaisyChainProcessRange)
	}

	void IPointsProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void IPointsProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void IPointsProcessor::OnRangeProcessingComplete()
	{
	}

	void IPointsProcessor::CompleteWork()
	{
	}

	void IPointsProcessor::Write()
	{
	}

	void IPointsProcessor::Output()
	{
	}

	void IPointsProcessor::Cleanup()
	{
		bIsProcessorValid = false;
	}

	bool IPointsProcessor::InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
	}

	int32 IPointsProcessor::FilterScope(const PCGExMT::FScope& Scope)
	{
		if (PrimaryFilters) { return PrimaryFilters->Test(Scope, PointFilterCache); }
		return DefaultPointFilterValue ? Scope.Count : 0;
	}

	int32 IPointsProcessor::FilterAll()
	{
		return FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum()));
	}

	IPointsProcessorBatch::IPointsProcessorBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: ExecutionContext(InContext), PointsCollection(InPointsCollection)
	{
		PCGEX_LOG_CTR(IPointsProcessorBatch)
		SetExecutionContext(InContext);
	}

	void IPointsProcessorBatch::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	bool IPointsProcessorBatch::PrepareProcessing()
	{
		return true;
	}

	void IPointsProcessorBatch::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
	}

	void IPointsProcessorBatch::OnInitialPostProcess()
	{
	}

	void IPointsProcessorBatch::CompleteWork()
	{
	}

	void IPointsProcessorBatch::Write()
	{
	}

	void IPointsProcessorBatch::Output()
	{
	}

	void IPointsProcessorBatch::Cleanup()
	{
		ProcessorFacades.Empty();
	}

	void IPointsProcessorBatch::InternalInitProcessor(const TSharedPtr<IPointsProcessor>& InProcessor, const int32 InIndex)
	{
		InProcessor->SetExecutionContext(ExecutionContext);
		InProcessor->ParentBatch = SharedThis(this);
		InProcessor->BatchIndex = InIndex;

		if (FilterFactories) { InProcessor->SetPointsFilterData(FilterFactories); }
		if (PrimaryInstancedFactory) { InProcessor->PrimaryInstancedFactory = PrimaryInstancedFactory; }

		InProcessor->RegisterConsumableAttributesWithFacade();
	}
}
