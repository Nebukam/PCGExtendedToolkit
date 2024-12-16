// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "PCGExOperation.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

namespace PCGExPointsMT
{
	PCGEX_CTX_STATE(MTState_PointsProcessing)
	PCGEX_CTX_STATE(MTState_PointsCompletingWork)
	PCGEX_CTX_STATE(MTState_PointsWriting)

#define PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, _BODY)\
	if (_INLINE_CONDITION)  { \
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Inlined) \
		_ID##Inlined->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS const TSharedRef<T>& Processor = This->Processors[Index]; _BODY }; \
		_ID##Inlined->StartIterations( Processors.Num(), 1, true);\
	} else {\
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##NonTrivial)\
		_ID##NonTrivial->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS \
		const TSharedRef<T>& Processor = This->Processors[Index]; if (Processor->IsTrivial()) { return; } _BODY }; \
		_ID##NonTrivial->StartIterations(Processors.Num(), 1, false);\
		if(!TrivialProcessors.IsEmpty()){ PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Trivial) \
		_ID##Trivial->OnIterationCallback =[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope){ PCGEX_ASYNC_THIS const TSharedRef<T>& Processor = This->TrivialProcessors[Index]; _BODY }; \
		_ID##Trivial->StartIterations( TrivialProcessors.Num(), 32, false); }\
	}

#define PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, _PLI) \
	if (IsTrivial()){ _PREPARE({PCGExMT::FScope(0, _NUM, 0)}); _PROCESS(PCGExMT::FScope(0, _NUM, 0)); _COMPLETE(); return; } \
	const int32 PLI = GetDefault<UPCGExGlobalSettings>()->_PLI(PerLoopIterations); \
	PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopFor##_NAME) \
	ParallelLoopFor##_NAME->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]() { PCGEX_ASYNC_THIS This->_COMPLETE(); }; \
	ParallelLoopFor##_NAME->OnPrepareSubLoopsCallback = [PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops) { PCGEX_ASYNC_THIS This->_PREPARE(Loops); }; \
	ParallelLoopFor##_NAME->OnSubLoopStartCallback =[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS This->_PROCESS(Scope); }; \
    ParallelLoopFor##_NAME->StartSubLoops(_NUM, PLI, _INLINE);

#define PCGEX_ASYNC_POINT_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetPointsBatchChunkSize)

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY })

	class FPointsProcessorBatchBase;

	class FPointsProcessor : public TSharedFromThis<FPointsProcessor>
	{
		friend class FPointsProcessorBatchBase;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;

		TSharedPtr<PCGExData::FFacadePreloader> InternalFacadePreloader;

		TSharedPtr<PCGExPointFilter::FManager> PrimaryFilters;
		bool bInlineProcessPoints = false;
		bool bDaisyChainProcessRange = false;

		PCGExData::ESource CurrentProcessingSource = PCGExData::ESource::Out;
		int32 LocalPointProcessingChunkSize = -1;

	public:
		TWeakPtr<FPointsProcessorBatchBase> ParentBatch;
		TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager() { return AsyncManager; }

		bool bIsProcessorValid = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsTrivial = false;

		TArray<int8> PointFilterCache;

		int32 BatchIndex = -1;

		UPCGExOperation* PrimaryOperation = nullptr;

		explicit FPointsProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			PointDataFacade(InPointDataFacade)
		{
			PCGEX_LOG_CTR(FPointsProcessor)
		}

		virtual void SetExecutionContext(FPCGExContext* InContext)
		{
			check(InContext)
			ExecutionContext = InContext;
		}

		virtual ~FPointsProcessor()
		{
			PCGEX_LOG_DTR(FPointsProcessor)
		}

		virtual bool IsTrivial() const { return bIsTrivial; }

		bool HasFilters() const { return FilterFactories != nullptr; }

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFactories)
		{
			FilterFactories = InFactories;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
		{
			if (HasFilters()) { PCGExPointFilter::RegisterBuffersDependencies(ExecutionContext, *FilterFactories, FacadePreloader); }
		}

		void PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup)
		{
			AsyncManager = InAsyncManager;

			InternalFacadePreloader = MakeShared<PCGExData::FFacadePreloader>();
			RegisterBuffersDependencies(*InternalFacadePreloader);

			InternalFacadePreloader->StartLoading(AsyncManager, PointDataFacade, InPrefetchDataTaskGroup);
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
		{
			AsyncManager = InAsyncManager;

#pragma region Path filter data

			if (FilterFactories) { InitPrimaryFilters(FilterFactories); }

#pragma endregion

			if (PrimaryOperation)
			{
				PrimaryOperation = PrimaryOperation->CopyOperation<UPCGExOperation>();
				PrimaryOperation->PrimaryDataFacade = PointDataFacade;
			}

			return true;
		}

#pragma region Parallel loop for points

		void StartParallelLoopForPoints(const PCGExData::ESource Source = PCGExData::ESource::Out, const int32 PerLoopIterations = -1)
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

		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
		{
		}

		virtual void ProcessPoints(const PCGExMT::FScope& Scope)
		{
			if (!PointDataFacade->IsDataValid(CurrentProcessingSource)) { return; }

			PrepareSingleLoopScopeForPoints(Scope);
			TArray<FPCGPoint>& Points = PointDataFacade->Source->GetMutableData(CurrentProcessingSource)->GetMutablePoints();
			for (int i = Scope.Start; i < Scope.End; i++) { ProcessSinglePoint(i, Points[i], Scope); }
		}

		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
		{
		}

		virtual void OnPointsProcessingComplete()
		{
		}

#pragma endregion

#pragma region Parallel loop for Range

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			PCGEX_ASYNC_POINT_PROCESSOR_LOOP(
				Ranges, NumIterations,	
				PrepareLoopScopesForRanges, ProcessRange,
				OnRangeProcessingComplete,
				bDaisyChainProcessRange)
		}

		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForRange(const PCGExMT::FScope& Scope)
		{
		}

		virtual void ProcessRange(const PCGExMT::FScope& Scope)
		{
			PrepareSingleLoopScopeForRange(Scope);
			for (int i = Scope.Start; i < Scope.End; i++) { ProcessSingleRangeIteration(i, Scope); }
		}

		virtual void OnRangeProcessingComplete()
		{
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope)
		{
		}

#pragma endregion

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}

		virtual void Output()
		{
		}

		virtual void Cleanup()
		{
			bIsProcessorValid = false;
		}

	protected:
		virtual bool InitPrimaryFilters(TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories)
		{
			PointFilterCache.Init(DefaultPointFilterValue, PointDataFacade->GetNum());

			if (InFilterFactories->IsEmpty()) { return true; }

			PrimaryFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
		}

		virtual void FilterScope(const PCGExMT::FScope& Scope)
		{
			if (PrimaryFilters) { for (int i = Scope.Start; i < Scope.End; i++) { PointFilterCache[i] = PrimaryFilters->Test(i); } }
		}

		virtual void FilterAll() { FilterScope(PCGExMT::FScope(0, PointDataFacade->GetNum())); }
	};

	template <typename TContext, typename TSettings>
	class TPointsProcessor : public FPointsProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		explicit TPointsProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			FPointsProcessor(InPointDataFacade)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			FPointsProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
			check(Context)
			check(Settings)
		}

		FORCEINLINE TContext* GetContext() { return Context; }
		FORCEINLINE const TSettings* GetSettings() { return Settings; }
	};

	class FPointsProcessorBatchBase : public TSharedFromThis<FPointsProcessorBatchBase>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* FilterFactories = nullptr;

	public:
		bool bPrefetchData = false;
		bool bDaisyChainProcessing = false;
		bool bDaisyChainCompletion = false;
		bool bDaisyChainWrite = false;
		bool bRequiresWriteStep = false;
		TArray<TSharedRef<PCGExData::FFacade>> ProcessorFacades;
		TMap<PCGExData::FPointIO*, TSharedRef<FPointsProcessor>>* SubProcessorMap = nullptr;

		mutable FRWLock BatchLock;

		PCGEx::ContextState CurrentState = PCGEx::State_InitialExecution;

		FPCGExContext* ExecutionContext = nullptr;

		TArray<TWeakPtr<PCGExData::FPointIO>> PointsCollection;

		UPCGExOperation* PrimaryOperation = nullptr;

		virtual int32 GetNumProcessors() const { return -1; }

		FPointsProcessorBatchBase(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection):
			ExecutionContext(InContext), PointsCollection(InPointsCollection)
		{
			PCGEX_LOG_CTR(FPointsProcessorBatchBase)
		}

		virtual ~FPointsProcessorBatchBase()
		{
			PCGEX_LOG_DTR(FPointsProcessorBatchBase)
		}

		virtual void SetExecutionContext(FPCGExContext* InContext)
		{
			ExecutionContext = InContext;
		}

		virtual bool PrepareProcessing()
		{
			return true;
		}

		virtual void Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}

		virtual void Output()
		{
		}

		virtual void Cleanup()
		{
			ProcessorFacades.Empty();
		}
	};

	template <typename T>
	class TBatch : public FPointsProcessorBatchBase
	{
	public:
		TArray<TSharedRef<T>> Processors;
		TArray<TSharedRef<T>> TrivialProcessors;

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		PCGEx::ContextState CurrentState = PCGEx::State_InitialExecution;

		TBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection):
			FPointsProcessorBatchBase(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
		}

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories)
		{
			FilterFactories = InFilterFactories;
		}

		virtual bool PrepareProcessing() override
		{
			return FPointsProcessorBatchBase::PrepareProcessing();
		}

		virtual void Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override
		{
			if (PointsCollection.IsEmpty()) { return; }

			CurrentState = PCGEx::State_Processing;

			AsyncManager = InAsyncManager;

			TSharedPtr<FPointsProcessorBatchBase> SelfPtr = SharedThis(this);

			for (const TWeakPtr<PCGExData::FPointIO>& WeakIO : PointsCollection)
			{
				TSharedPtr<PCGExData::FPointIO> IO = WeakIO.Pin();

				PCGEX_MAKE_SHARED(PointDataFacade, PCGExData::FFacade, IO.ToSharedRef())

				const TSharedPtr<T> NewProcessor = MakeShared<T>(PointDataFacade.ToSharedRef());

				NewProcessor->SetExecutionContext(ExecutionContext);
				NewProcessor->ParentBatch = SelfPtr;
				NewProcessor->BatchIndex = Processors.Num();

				if (!PrepareSingle(NewProcessor)) { continue; }
				Processors.Add(NewProcessor.ToSharedRef());

				ProcessorFacades.Add(NewProcessor->PointDataFacade);
				SubProcessorMap->Add(&NewProcessor->PointDataFacade->Source.Get(), NewProcessor.ToSharedRef());

				if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
				if (PrimaryOperation) { NewProcessor->PrimaryOperation = PrimaryOperation; }


				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor.ToSharedRef()); }
			}

			if (bPrefetchData)
			{
				PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelAttributeRead)

				ParallelAttributeRead->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnProcessingPreparationComplete();
				};

				ParallelAttributeRead->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE, ParallelAttributeRead](const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						This->Processors[Scope.Start]->PrefetchData(This->AsyncManager, ParallelAttributeRead);
					};

				ParallelAttributeRead->StartSubLoops(Processors.Num(), 1);
			}
			else
			{
				OnProcessingPreparationComplete();
			}
		}

	protected:
		void OnProcessingPreparationComplete()
		{
			PCGEX_ASYNC_MT_LOOP_TPL(Process, bDaisyChainProcessing, { Processor->bIsProcessorValid = Processor->Process(This->AsyncManager); })
		}

	public:
		virtual bool PrepareSingle(const TSharedPtr<T>& PointsProcessor)
		{
			return true;
		};

		virtual void CompleteWork() override
		{
			CurrentState = PCGEx::State_Completing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bDaisyChainCompletion, { Processor->CompleteWork(); })
			FPointsProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			CurrentState = PCGEx::State_Writing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bDaisyChainWrite, { Processor->Write(); })
			FPointsProcessorBatchBase::Write();
		}

		virtual void Output() override
		{
			for (const TSharedPtr<T>& P : Processors)
			{
				if (!P->bIsProcessorValid) { continue; }
				P->Output();
			}
		}

		virtual void Cleanup() override
		{
			FPointsProcessorBatchBase::Cleanup();
			for (const TSharedPtr<T>& P : Processors) { P->Cleanup(); }
			Processors.Empty();
		}
	};
}
