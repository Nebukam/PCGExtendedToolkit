// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "PCGExOperation.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"


#include "Graph/PCGExGraph.h"

namespace PCGExPointsMT
{
	PCGEX_ASYNC_STATE(MTState_PointsProcessing)
	PCGEX_ASYNC_STATE(MTState_PointsCompletingWork)
	PCGEX_ASYNC_STATE(MTState_PointsWriting)

	PCGEX_ASYNC_STATE(State_PointsAsyncWorkComplete)

#define PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, _BODY)\
	if (_INLINE_CONDITION)  { \
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Inlined) \
		_ID##Inlined->StartRanges( \
			[&](const int32 Index, const int32 Count, const int32 LoopIdx) { \
				T* Processor = Processors[Index]; _BODY \
			}, Processors.Num(), 1, true, false);\
	} else {\
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##NonTrivial)\
		_ID##NonTrivial->StartRanges(\
			[&](const int32 Index, const int32 Count, const int32 LoopIdx) {\
				T* Processor = Processors[Index];\
				if (Processor->IsTrivial()) { return; } _BODY \
			}, Processors.Num(), 1, false, false); \
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, _ID##Trivial) \
		_ID##Trivial->StartRanges(\
			[&](const int32 Index, const int32 Count, const int32 LoopIdx){ \
				T* Processor = TrivialProcessors[Index]; _BODY \
			}, TrivialProcessors.Num(), 32, false, false); \
	}

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY })

	class FPointsProcessorBatchBase;

	class FPointsProcessor : public TSharedFromThis<FPointsProcessor>
	{
		friend class FPointsProcessorBatchBase;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;

		TUniquePtr<PCGExPointFilter::TManager> PrimaryFilters;
		bool bInlineProcessPoints = false;
		bool bInlineProcessRange = false;

		PCGExData::ESource CurrentProcessingSource = PCGExData::ESource::Out;

	public:
		TSharedPtr<FPointsProcessorBatchBase> ParentBatch;

		bool bIsProcessorValid = false;

		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsTrivial = false;

		TArray<bool> PointFilterCache;


		TSharedPtr<PCGExData::FPointIO> PointIO;
		int32 BatchIndex = -1;

		UPCGExOperation* PrimaryOperation = nullptr;

		explicit FPointsProcessor(const TSharedPtr<PCGExData::FPointIO>& InPoints):
			PointIO(InPoints)
		{
			PCGEX_LOG_CTR(FPointsProcessor)
			PointDataFacade = MakeShared<PCGExData::FFacade>(InPoints);
		}

		virtual void SetExecutionContext(FPCGExContext* InContext)
		{
			ExecutionContext = InContext;
		}

		virtual ~FPointsProcessor()
		{
			PCGEX_LOG_DTR(FPointsProcessor)

			PointIO = nullptr;
			PCGEX_DELETE_OPERATION(PrimaryOperation)

			PointFilterCache.Empty();
		}

		virtual bool IsTrivial() const { return bIsTrivial; }

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFactories)
		{
			FilterFactories = InFactories;
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
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
			const int32 NumPoints = PointIO->GetNum(Source);

			if (IsTrivial())
			{
				PrepareLoopScopesForPoints({PCGEx::H64(0, NumPoints)});
				ProcessPoints(0, NumPoints, 0);
				OnPointsProcessingComplete();
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(PerLoopIterations);

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopForPoints)
			ParallelLoopForPoints->SetOnCompleteCallback([&]() { OnPointsProcessingComplete(); });
			ParallelLoopForPoints->SetOnIterationRangePrepareCallback([&](const TArray<uint64>& Loops) { PrepareLoopScopesForPoints(Loops); });
			ParallelLoopForPoints->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessPoints(StartIndex, Count, LoopIdx); });
			ParallelLoopForPoints->PrepareRangesOnly(NumPoints, PLI, bInlineProcessPoints);
		}

		virtual void PrepareLoopScopesForPoints(const TArray<uint64>& Loops)
		{
		}

		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
		{
		}

		virtual void ProcessPoints(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			PrepareSingleLoopScopeForPoints(StartIndex, Count);
			TArray<FPCGPoint>& Points = PointIO->GetMutableData(CurrentProcessingSource)->GetMutablePoints();
			for (int i = 0; i < Count; ++i)
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
			ParallelLoopForRanges->SetOnCompleteCallback([&]() { OnRangeProcessingComplete(); });
			ParallelLoopForRanges->SetOnIterationRangePrepareCallback([&](const TArray<uint64>& Loops) { PrepareLoopScopesForRanges(Loops); });
			ParallelLoopForRanges->SetOnIterationRangeStartCallback(
				[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx) { ProcessRange(StartIndex, Count, LoopIdx); });
			ParallelLoopForRanges->PrepareRangesOnly(NumIterations, PLI, bInlineProcessRange);
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
			for (int i = 0; i < Count; ++i) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Count); }
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

	protected:
		virtual bool InitPrimaryFilters(TArray<UPCGExFilterFactoryBase*>* InFilterFactories)
		{
			PointFilterCache.Init(DefaultPointFilterValue, PointIO->GetNum());

			if (InFilterFactories->IsEmpty()) { return true; }

			PrimaryFilters = MakeUnique<PCGExPointFilter::TManager>(PointDataFacade);
			return PrimaryFilters->Init(ExecutionContext, *InFilterFactories);
		}

		virtual void FilterScope(const int32 StartIndex, const int32 Count)
		{
			if (PrimaryFilters)
			{
				const int32 MaxIndex = StartIndex + Count;
				for (int i = StartIndex; i < MaxIndex; ++i) { PointFilterCache[i] = PrimaryFilters->Test(i); }
			}
		}

		virtual void FilterAll() { FilterScope(0, PointIO->GetNum()); }
	};

	template <typename TContext, typename TSettings>
	class TPointsProcessor : public FPointsProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		TPointsProcessor(const TSharedPtr<PCGExData::FPointIO>& InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			FPointsProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
		}

		FORCEINLINE TContext* GetContext() { return Context; }
		FORCEINLINE const TSettings* GetSettings() { return Settings; }
	};

	class FPointsProcessorBatchBase : public TSharedFromThis<FPointsProcessorBatchBase>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;

	public:
		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bInlineWrite = false;
		bool bRequiresWriteStep = false;
		TArray<PCGExData::FFacade*> ProcessorFacades;
		TMap<PCGExData::FPointIO*, FPointsProcessor*>* SubProcessorMap = nullptr;

		mutable FRWLock BatchLock;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		FPCGExContext* ExecutionContext = nullptr;

		TArray<PCGExData::FPointIO*> PointsCollection;

		UPCGExOperation* PrimaryOperation = nullptr;

		virtual int32 GetNumProcessors() const { return -1; }

		FPointsProcessorBatchBase(FPCGExContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			ExecutionContext(InContext), PointsCollection(InPointsCollection)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext)
		{
			ExecutionContext = InContext;
		}

		virtual ~FPointsProcessorBatchBase()
		{
			ExecutionContext = nullptr;
			PointsCollection.Empty();
			ProcessorFacades.Empty();
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
	};

	template <typename T>
	class TBatch : public FPointsProcessorBatchBase
	{
	public:
		TArray<T*> Processors;
		TArray<T*> TrivialProcessors;

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		TBatch(FPCGExContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			FPointsProcessorBatchBase(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
			TrivialProcessors.Empty();
			PCGEX_DELETE_TARRAY(Processors)
		}

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFilterFactories)
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

			CurrentState = PCGExMT::State_Processing;

			AsyncManager = InAsyncManager;

			for (PCGExData::FPointIO* IO : PointsCollection)
			{
				IO->CreateInKeys();

				TSharedPtr<T> NewProcessor = Processors.Add_GetRef(MakeShared<T>(IO));
				NewProcessor->SetExecutionContext(ExecutionContext);
				NewProcessor->ParentBatch = SharedThis(this);
				NewProcessor->BatchIndex = Processors.Num() - 1;

				if (!PrepareSingle(NewProcessor))
				{
					Processors.Pop();
					continue;
				}

				ProcessorFacades.Add(NewProcessor->PointDataFacade.Get());
				SubProcessorMap->Add(NewProcessor->PointDataFacade->Source, NewProcessor);

				if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
				if (PrimaryOperation) { NewProcessor->PrimaryOperation = PrimaryOperation; }


				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor); }
			}

			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(AsyncManager); })
		}

		virtual bool PrepareSingle(T* PointsProcessor)
		{
			return true;
		};

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bInlineCompletion, { Processor->CompleteWork(); })
			FPointsProcessorBatchBase::CompleteWork();
		}

		virtual void Write() override
		{
			CurrentState = PCGExMT::State_Writing;
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bInlineWrite, { Processor->Write(); })
			FPointsProcessorBatchBase::Write();
		}

		virtual void Output() override
		{
			for (T* Processor : Processors)
			{
				if (!Processor->bIsProcessorValid) { continue; }
				Processor->Output();
			}
		}
	};
}
