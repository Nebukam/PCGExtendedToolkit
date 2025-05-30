// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsMT.h"

#include "PCGExInstancedFactory.h"


namespace PCGExPointsMT
{
	FPointsProcessor::FPointsProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
		PCGEX_LOG_CTR(FPointsProcessor)
	}

	void FPointsProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		check(InContext)
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	void FPointsProcessor::SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFactories)
	{
		FilterFactories = InFactories;
	}

	void FPointsProcessor::RegisterConsumableAttributesWithFacade() const
	{
		// Gives an opportunity for the processor to register attributes with a valid facade
		// So selectors shortcut can be properly resolved (@Last, etc.)

		if (FilterFactories) { PCGExFactories::RegisterConsumableAttributesWithFacade(*FilterFactories, PointDataFacade); }
		if (PrimaryInstancedFactory) { PrimaryInstancedFactory->RegisterConsumableAttributesWithFacade(ExecutionContext, PointDataFacade); }
	}

	void FPointsProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (HasFilters()) { PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *FilterFactories, FacadePreloader); }
	}

	void FPointsProcessor::PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
	{
		AsyncManager = InAsyncManager;

		InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>(PointDataFacade);
		RegisterBuffersDependencies(*InternalFacadePreloader);

		InternalFacadePreloader->StartLoading(AsyncManager, InPrefetchDataTaskGroup);
	}

	bool FPointsProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
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
				PrimaryInstancedFactory = PrimaryInstancedFactory->CreateNewInstance<UPCGExInstancedFactory>();
				PrimaryInstancedFactory->PrimaryDataFacade = PointDataFacade;
			}
		}

		return true;
	}

	void FPointsProcessor::StartParallelLoopForPoints(const PCGExData::EIOSide Side, const int32 PerLoopIterations)
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

	void FPointsProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FPointsProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
	}

	void FPointsProcessor::OnPointsProcessingComplete()
	{
	}

	void FPointsProcessor::StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations)
	{
		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Ranges, NumIterations,
			PrepareLoopScopesForRanges, ProcessRange,
			OnRangeProcessingComplete,
			bDaisyChainProcessRange)
	}

	void FPointsProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FPointsProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
	}

	void FPointsProcessor::OnRangeProcessingComplete()
	{
	}

	void FPointsProcessor::CompleteWork()
	{
	}

	void FPointsProcessor::Write()
	{
	}

	void FPointsProcessor::Output()
	{
	}

	void FPointsProcessor::Cleanup()
	{
		bIsProcessorValid = false;
	}

	bool FPointsProcessor::InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
	}

	void FPointsProcessor::FilterScope(const PCGExMT::FScope& Scope)
	{
		if (PrimaryFilters) { PrimaryFilters->Test(Scope, PointFilterCache); }
	}

	void FPointsProcessor::FilterAll()
	{
		FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum()));
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
}
