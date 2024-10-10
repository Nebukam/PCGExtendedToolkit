// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "PCGExOperation.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"

namespace PCGExPointsMT
{
	PCGEX_ASYNC_STATE(MTState_PointsProcessing)
	PCGEX_ASYNC_STATE(MTState_PointsCompletingWork)
	PCGEX_ASYNC_STATE(MTState_PointsWriting)

#define PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, _BODY)\
	if (_INLINE_CONDITION)  { \
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Inlined) \
		_ID##Inlined->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { \
		const TSharedRef<T>& Processor = Processors[Index]; _BODY }; \
		_ID##Inlined->StartIterations( Processors.Num(), 1, true, false);\
	} else {\
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##NonTrivial)\
		_ID##NonTrivial->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) {\
		const TSharedRef<T>& Processor = Processors[Index]; if (Processor->IsTrivial()) { return; } _BODY }; \
		_ID##NonTrivial->StartIterations(Processors.Num(), 1, false, false);\
		if(!TrivialProcessors.IsEmpty()){\
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Trivial) \
		_ID##Trivial->OnIterationCallback =[&](const int32 Index, const int32 Count, const int32 LoopIdx){ \
		const TSharedRef<T>& Processor = TrivialProcessors[Index]; _BODY }; \
		_ID##Trivial->StartIterations( TrivialProcessors.Num(), 32, false, false); }\
	}

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY })

	class FPointsProcessorBatchBase;

	class FPointsProcessor : public TSharedFromThis<FPointsProcessor>
	{
		friend class FPointsProcessorBatchBase;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;

		TSharedPtr<PCGExPointFilter::FManager> PrimaryFilters;
		bool bInlineProcessPoints = false;
		bool bInlineProcessRange = false;

		PCGExData::ESource CurrentProcessingSource = PCGExData::ESource::Out;

	public:
		TWeakPtr<FPointsProcessorBatchBase> ParentBatch;
		TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager() { return AsyncManager; }

		bool bIsProcessorValid = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsTrivial = false;

		TArray<bool> PointFilterCache;

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

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFactories)
		{
			FilterFactories = InFactories;
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

#pragma region Simple range loop

		void StartParallelLoopForPoints(const PCGExData::ESource Source = PCGExData::ESource::Out, const int32 PerLoopIterations = -1)
		{
			CurrentProcessingSource = Source;

			if (!PointDataFacade->IsDataValid(CurrentProcessingSource)) { return; }

			const int32 NumPoints = PointDataFacade->Source->GetNum(Source);

			if (IsTrivial())
			{
				PrepareLoopScopesForPoints({PCGEx::H64(0, NumPoints)});
				ProcessPoints(0, NumPoints, 0);
				OnPointsProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForPoints)
			ParallelLoopForPoints->OnCompleteCallback = [&]() { OnPointsProcessingComplete(); };
			ParallelLoopForPoints->OnIterationRangePrepareCallback = [&](const TArray<uint64>& Loops) { PrepareLoopScopesForPoints(Loops); };
			ParallelLoopForPoints->OnIterationRangeStartCallback =
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessPoints(StartIndex, Count, LoopIdx); };
			ParallelLoopForPoints->StartRangePrepareOnly(NumPoints, PLI, bInlineProcessPoints);
		}

		virtual void PrepareLoopScopesForPoints(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessPoints(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			if (!PointDataFacade->IsDataValid(CurrentProcessingSource)) { return; }

			PrepareSingleLoopScopeForPoints(StartIndex, Count);
			TArray<FPCGPoint>& Points = PointDataFacade->Source->GetMutableData(CurrentProcessingSource)->GetMutablePoints();
			for (int i = 0; i < Count; i++)
			{
				const int32 PtIndex = StartIndex + i;
				ProcessSinglePoint(PtIndex, Points[PtIndex], LoopIdx, Count);
			}
		}

		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
		{
		}

		virtual void OnPointsProcessingComplete()
		{
		}

#pragma endregion

#pragma region Simple range loop

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				PrepareLoopScopesForRanges({PCGEx::H64(0, NumIterations)});
				ProcessRange(0, NumIterations, 0);
				OnRangeProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForRanges)
			ParallelLoopForRanges->OnCompleteCallback = [&]() { OnRangeProcessingComplete(); };
			ParallelLoopForRanges->OnIterationRangePrepareCallback = [&](const TArray<uint64>& Loops) { PrepareLoopScopesForRanges(Loops); };
			ParallelLoopForRanges->OnIterationRangeStartCallback =
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessRange(StartIndex, Count, LoopIdx); };
			ParallelLoopForRanges->StartRangePrepareOnly(NumIterations, PLI, bInlineProcessRange);
		}

		virtual void PrepareLoopScopesForRanges(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForRange(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessRange(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			PrepareSingleLoopScopeForRange(StartIndex, Count);
			for (int i = 0; i < Count; i++) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Count); }
		}

		virtual void OnRangeProcessingComplete()
		{
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
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

		virtual void FilterScope(const int32 StartIndex, const int32 Count)
		{
			if (PrimaryFilters)
			{
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; i++) { PointFilterCache[i] = PrimaryFilters->Test(i); }
			}
		}

		virtual void FilterAll() { FilterScope(0, PointDataFacade->GetNum()); }
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
		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bInlineWrite = false;
		bool bRequiresWriteStep = false;
		TArray<TSharedRef<PCGExData::FFacade>> ProcessorFacades;
		TMap<PCGExData::FPointIO*, TSharedRef<FPointsProcessor>>* SubProcessorMap = nullptr;

		mutable FRWLock BatchLock;

		PCGEx::AsyncState CurrentState = PCGEx::State_InitialExecution;

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

		PCGEx::AsyncState CurrentState = PCGEx::State_InitialExecution;

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
				const TSharedPtr<PCGExData::FFacade> PointDataFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
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

			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(AsyncManager); })
		}

		virtual bool PrepareSingle(const TSharedPtr<T>& PointsProcessor)
		{
			return true;
		};

		virtual void CompleteWork() override
		{
			CurrentState = PCGEx::State_Completing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bInlineCompletion, { Processor->CompleteWork(); })
			FPointsProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			CurrentState = PCGEx::State_Writing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bInlineWrite, { Processor->Write(); })
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
