// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPointsMT.h"


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
		if (PrimaryOperation) { PrimaryOperation->RegisterConsumableAttributesWithFacade(ExecutionContext, PointDataFacade); }
	}

	void FPointsProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		if (HasFilters()) { PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *FilterFactories, FacadePreloader); }
	}

	void FPointsProcessor::PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
	{
		AsyncManager = InAsyncManager;

		InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>();
		RegisterBuffersDependencies(*InternalFacadePreloader);

		InternalFacadePreloader->StartLoading(AsyncManager, PointDataFacade, InPrefetchDataTaskGroup);
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

		if (PrimaryOperation)
		{
			PrimaryOperation = PrimaryOperation->CopyOperation<UPCGExOperation>();
			PrimaryOperation->PrimaryDataFacade = PointDataFacade;
		}

		return true;
	}

	void FPointsProcessor::StartParallelLoopForPoints(const PCGExData::ESource Source, const int32 PerLoopIterations)
	{
		CurrentProcessingSource = Source;

		if (!PointDataFacade->IsDataValid(CurrentProcessingSource)) { return; }

		const int32 NumPoints = PointDataFacade->Source->GetNum(Source);

		PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
			Points, NumPoints,
			PrepareLoopScopesForPoints, ProcessPoints,
			OnPointsProcessingComplete,
			bInlineProcessPoints)
	}

	void FPointsProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
	}

	void FPointsProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
	}

	void FPointsProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		if (!PointDataFacade->IsDataValid(CurrentProcessingSource)) { return; }

		PrepareSingleLoopScopeForPoints(Scope);
		TArray<FPCGPoint>& Points = PointDataFacade->Source->GetMutableData(CurrentProcessingSource)->GetMutablePoints();
		for (int i = Scope.Start; i < Scope.End; i++) { ProcessSinglePoint(i, Points[i], Scope); }
	}

	void FPointsProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
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

	void FPointsProcessor::PrepareSingleLoopScopeForRange(const PCGExMT::FScope& Scope)
	{
	}

	void FPointsProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PrepareSingleLoopScopeForRange(Scope);
		for (int i = Scope.Start; i < Scope.End; i++) { ProcessSingleRangeIteration(i, Scope); }
	}

	void FPointsProcessor::OnRangeProcessingComplete()
	{
	}

	void FPointsProcessor::ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
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

	bool FPointsProcessor::InitPrimaryFilters(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
	{
		PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

		if (InFilterFactories->IsEmpty()) { return true; }

		PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
		return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
	}

	void FPointsProcessor::FilterScope(const PCGExMT::FScope& Scope)
	{
		if (PrimaryFilters) { for (int i = Scope.Start; i < Scope.End; i++) { PointFilterCache[i] = PrimaryFilters->Test(i); } }
	}

	void FPointsProcessor::FilterAll()
	{
		FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum()));
	}

	FPointsProcessorBatchBase::FPointsProcessorBatchBase(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: ExecutionContext(InContext), PointsCollection(InPointsCollection)
	{
		PCGEX_LOG_CTR(FPointsProcessorBatchBase)
		SetExecutionContext(InContext);
	}

	void FPointsProcessorBatchBase::SetExecutionContext(FPCGExContext* InContext)
	{
		ExecutionContext = InContext;
		WorkPermit = ExecutionContext->GetWorkPermit();
	}

	bool FPointsProcessorBatchBase::PrepareProcessing()
	{
		return true;
	}

	void FPointsProcessorBatchBase::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
	}

	void FPointsProcessorBatchBase::CompleteWork()
	{
	}

	void FPointsProcessorBatchBase::Write()
	{
	}

	void FPointsProcessorBatchBase::Output()
	{
	}

	void FPointsProcessorBatchBase::Cleanup()
	{
		ProcessorFacades.Empty();
	}
}
