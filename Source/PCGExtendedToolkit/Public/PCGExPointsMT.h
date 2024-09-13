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
		PCGEX_ASYNC_GROUP(AsyncManagerPtr, _ID##Inlined) \
		_ID##Inlined->StartRanges( \
			[&](const int32 Index, const int32 Count, const int32 LoopIdx) { \
				T* Processor = Processors[Index]; _BODY \
			}, Processors.Num(), 1, true, false);\
	} else {\
		PCGEX_ASYNC_GROUP(AsyncManagerPtr, _ID##NonTrivial)\
		_ID##NonTrivial->StartRanges(\
			[&](const int32 Index, const int32 Count, const int32 LoopIdx) {\
				T* Processor = Processors[Index];\
				if (Processor->IsTrivial()) { return; } _BODY \
			}, Processors.Num(), 1, false, false); \
		PCGEX_ASYNC_GROUP(AsyncManagerPtr, _ID##Trivial) \
		_ID##Trivial->StartRanges(\
			[&](const int32 Index, const int32 Count, const int32 LoopIdx){ \
				T* Processor = TrivialProcessors[Index]; _BODY \
			}, TrivialProcessors.Num(), 32, false, false); \
	}

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY })

	class FPointsProcessorBatchBase;

	class FPointsProcessor
	{
		friend class FPointsProcessorBatchBase;

	protected:
		PCGExPointFilter::TManager* PrimaryFilters = nullptr;
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;
		bool bInlineProcessPoints = false;
		bool bInlineProcessRange = false;

		PCGExData::ESource CurrentProcessingSource = PCGExData::ESource::Out;

	public:
		FPointsProcessorBatchBase* ParentBatch = nullptr;

		bool bIsProcessorValid = false;

		PCGExData::FFacade* PointDataFacade = nullptr;

		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsTrivial = false;

		TArray<bool> PointFilterCache;

		FPCGExContext* Context = nullptr;

		PCGExData::FPointIO* PointIO = nullptr;
		int32 BatchIndex = -1;

		UPCGExOperation* PrimaryOperation = nullptr;

		explicit FPointsProcessor(PCGExData::FPointIO* InPoints):
			PointIO(InPoints)
		{
			PCGEX_LOG_CTR(FPointsProcessor)
			PointDataFacade = new PCGExData::FFacade(InPoints);
		}

		virtual ~FPointsProcessor()
		{
			PCGEX_LOG_DTR(FPointsProcessor)

			PointIO = nullptr;
			PCGEX_DELETE_OPERATION(PrimaryOperation)

			PCGEX_DELETE(PrimaryFilters)
			PointFilterCache.Empty();
			PCGEX_DELETE(PointDataFacade)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		virtual bool IsTrivial() const { return bIsTrivial; }

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFactories)
		{
			FilterFactories = InFactories;
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

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

			PCGEX_ASYNC_GROUP(AsyncManagerPtr, ParallelLoopForPoints)
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

			PCGEX_ASYNC_GROUP(AsyncManagerPtr, ParallelLoopForRanges)
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

			PrimaryFilters = new PCGExPointFilter::TManager(PointDataFacade);
			return PrimaryFilters->Init(Context, *InFilterFactories);
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

	class FPointsProcessorBatchBase
	{
	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;
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

		FPCGExContext* Context = nullptr;

		TArray<PCGExData::FPointIO*> PointsCollection;

		UPCGExOperation* PrimaryOperation = nullptr;

		virtual int32 GetNumProcessors() const { return -1; }

		FPointsProcessorBatchBase(FPCGExContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			Context(InContext), PointsCollection(InPointsCollection)
		{
		}

		virtual ~FPointsProcessorBatchBase()
		{
			Context = nullptr;
			PointsCollection.Empty();
			ProcessorFacades.Empty();
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		virtual bool PrepareProcessing()
		{
			return true;
		}

		virtual void Process(PCGExMT::FTaskManager* AsyncManager)
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

		virtual void Process(PCGExMT::FTaskManager* AsyncManager) override
		{
			if (PointsCollection.IsEmpty()) { return; }

			CurrentState = PCGExMT::State_Processing;

			AsyncManagerPtr = AsyncManager;

			for (PCGExData::FPointIO* IO : PointsCollection)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(IO);
				NewProcessor->Context = Context;
				NewProcessor->ParentBatch = this;
				NewProcessor->BatchIndex = Processors.Add(NewProcessor);

				if (!PrepareSingle(NewProcessor))
				{
					Processors.Pop();
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				ProcessorFacades.Add(NewProcessor->PointDataFacade);
				SubProcessorMap->Add(NewProcessor->PointDataFacade->Source, NewProcessor);

				if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
				if (PrimaryOperation) { NewProcessor->PrimaryOperation = PrimaryOperation; }


				NewProcessor->bIsTrivial = IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize;
				if (NewProcessor->IsTrivial()) { TrivialProcessors.Add(NewProcessor); }
			}

			PCGEX_ASYNC_MT_LOOP_TPL(Process, bInlineProcessing, { Processor->bIsProcessorValid = Processor->Process(AsyncManagerPtr); })
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
