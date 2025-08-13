// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsMT.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExInstancedFactory.h"
#include "Data/PCGExDataPreloader.h"


namespace PCGExPointsMT
{
	IProcessor::IProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
		PCGEX_LOG_CTR(FPointsProcessor)
	}

	void IProcessor::SetExecutionContext(FPCGExContext* InContext)
	{
		check(InContext)
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	void IProcessor::SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFactories)
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

	void IProcessor::PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
	{
		AsyncManager = InAsyncManager;

		InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>(PointDataFacade);
		RegisterBuffersDependencies(*InternalFacadePreloader);

		InternalFacadePreloader->StartLoading(AsyncManager, InPrefetchDataTaskGroup);
	}

	bool IProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		AsyncManager = InAsyncManager;
		PCGEX_ASYNC_CHKD(AsyncManager)

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

		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Points, NumPoints,
			PrepareLoopScopesForPoints, ProcessPoints,
			OnPointsProcessingComplete,
			bDaisyChainProcessPoints)
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
		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Ranges, NumIterations,
			PrepareLoopScopesForRanges, ProcessRange,
			OnRangeProcessingComplete,
			bDaisyChainProcessRange)
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

	bool IProcessor::InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
	}

	int32 IProcessor::FilterScope(const PCGExMT::FScope& Scope)
	{
		if (PrimaryFilters) { return PrimaryFilters->Test(Scope, PointFilterCache); }
		return DefaultPointFilterValue ? Scope.Count : 0;
	}

	int32 IProcessor::FilterAll()
	{
		return FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum()));
	}

	IBatch::IBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: ExecutionContext(InContext), PointsCollection(InPointsCollection)
	{
		PCGEX_LOG_CTR(IBatch)
		SetExecutionContext(InContext);
	}

	void IBatch::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	bool IBatch::PrepareProcessing()
	{
		return true;
	}

	void IBatch::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
	}

	void IBatch::OnInitialPostProcess()
	{
	}

	void IBatch::CompleteWork()
	{
	}

	void IBatch::Write()
	{
	}

	void IBatch::Output()
	{
	}

	void IBatch::Cleanup()
	{
		ProcessorFacades.Empty();
	}

	void IBatch::InternalInitProcessor(const TSharedPtr<IProcessor>& InProcessor, const int32 InIndex)
	{
		InProcessor->SetExecutionContext(ExecutionContext);
		InProcessor->ParentBatch = SharedThis(this);
		InProcessor->BatchIndex = InIndex;

		if (FilterFactories) { InProcessor->SetPointsFilterData(FilterFactories); }
		if (PrimaryInstancedFactory) { InProcessor->PrimaryInstancedFactory = PrimaryInstancedFactory; }

		InProcessor->RegisterConsumableAttributesWithFacade();
	}
}
