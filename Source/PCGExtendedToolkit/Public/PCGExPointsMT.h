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
	enum class EMode : uint8
	{
		None     = 0,
		Process  = 1 << 0,
		Complete = 1 << 1,
		Write    = 1 << 2,
	};

	PCGEX_ASYNC_STATE(MTState_PointsProcessing)
	PCGEX_ASYNC_STATE(MTState_PointsCompletingWork)
	PCGEX_ASYNC_STATE(MTState_PointsWriting)

	PCGEX_ASYNC_STATE(State_PointsAsyncWorkComplete)

#pragma region Tasks

#define PCGEX_POINTS_MT_TASK(_NAME, _BODY)\
	template <typename T>\
	class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask	{\
		public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget) : PCGExMT::FPCGExTask(InPointIO),Target(InTarget){} \
		T* Target = nullptr; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_POINTS_MT_TASK_RANGE_INLINE(_NAME, _BODY)\
	template <typename T> \
	class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask {\
		public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const uint64 InPerNumIterations, const uint64 InTotalIterations, const PCGExData::ESource InSource = PCGExData::ESource::Out, const int32 InLoopIdx = 0)\
		: PCGExMT::FPCGExTask(InPointIO), Target(InTarget), PerNumIterations(InPerNumIterations), TotalIterations(InTotalIterations), Source(InSource), LoopIdx(InLoopIdx){}\
		T* Target = nullptr; uint64 PerNumIterations = 0; uint64 TotalIterations = 0; const PCGExData::ESource Source; const int32 LoopIdx;\
		virtual bool ExecuteTask() override {\
		const uint64 RemainingIterations = TotalIterations - TaskIndex;\
		uint64 Iterations = FMath::Min(PerNumIterations, RemainingIterations); _BODY \
		int32 NextIndex = TaskIndex + Iterations; if (NextIndex >= TotalIterations) { return true; }\
		InternalStart<_NAME>(NextIndex, nullptr, Target, PerNumIterations, TotalIterations, Source, LoopIdx+1);\
		return true; } };

#define PCGEX_POINTS_MT_TASK_RANGE(_NAME, _BODY)\
	template <typename T>\
		class PCGEXTENDEDTOOLKIT_API _NAME final : public PCGExMT::FPCGExTask	{\
			public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const int32 InIterations, const PCGExData::ESource InSource = PCGExData::ESource::Out, const int32 InLoopIdx = 0) \
			: PCGExMT::FPCGExTask(InPointIO),Target(InTarget), Iterations(InIterations), Source(InSource), LoopIdx(InLoopIdx){} \
			T* Target = nullptr; const int32 Iterations = 0; const PCGExData::ESource Source; const int32 LoopIdx; virtual bool ExecuteTask() override{_BODY return true; }}; \
	PCGEX_POINTS_MT_TASK_RANGE_INLINE(_NAME##Inline, _BODY)

	PCGEX_POINTS_MT_TASK(FStartPointsBatchProcessing, { if (Target->PrepareProcessing()) { Target->Process(Manager); } })

	PCGEX_POINTS_MT_TASK(FAsyncProcess, { Target->Process(Manager); })

	PCGEX_POINTS_MT_TASK(FAsyncProcessWithUpdate, { Target->bIsProcessorValid = Target->Process(Manager); })

	PCGEX_POINTS_MT_TASK(FAsyncCompleteWork, { Target->CompleteWork(); })

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncProcessPointRange, { Target->ProcessPoints(Source, TaskIndex, Iterations, LoopIdx);})

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncProcessRange, {Target->ProcessRange(TaskIndex, Iterations, LoopIdx);})

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncClosedBatchProcessRange, {Target->ProcessClosedBatchRange(TaskIndex, Iterations, LoopIdx);})

#pragma endregion

	class FPointsProcessorBatchBase;

	class FPointsProcessor
	{
		friend class FPointsProcessorBatchBase;

	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;
		bool bInlineProcessPoints = false;
		bool bInlineProcessRange = false;

	public:
		FPointsProcessorBatchBase* ParentBatch = nullptr;

		bool bIsProcessorValid = false;

		PCGExData::FFacade* PointDataFacade = nullptr;

		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsSmallPoints = false;

		TArray<bool> PointFilterCache;

		FPCGContext* Context = nullptr;

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

			PointFilterCache.Empty();
			PCGEX_DELETE(PointDataFacade)
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		bool IsTrivial() const { return bIsSmallPoints; }

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFactories)
		{
			FilterFactories = InFactories;
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

#pragma region Path filter data

			if (FilterFactories)
			{
				PCGEX_SET_NUM_UNINITIALIZED(PointFilterCache, PointIO->GetNum())

				if (FilterFactories->IsEmpty())
				{
					for (int i = 0; i < PointIO->GetNum(); i++) { PointFilterCache[i] = DefaultPointFilterValue; }
				}
				else
				{
					PCGExPointFilter::TManager* FilterManager = new PCGExPointFilter::TManager(PointDataFacade);
					FilterManager->Init(Context, *FilterFactories);

					for (int i = 0; i < PointIO->GetNum(); i++) { PointFilterCache[i] = FilterManager->Test(i); }
					PCGEX_DELETE(FilterManager)
				}
			}

#pragma endregion

			if (PrimaryOperation)
			{
				PrimaryOperation = PrimaryOperation->CopyOperation<UPCGExOperation>();
				PrimaryOperation->PrimaryDataFacade = PointDataFacade;
			}

			return true;
		}

		void StartParallelLoopForPoints(const PCGExData::ESource Source = PCGExData::ESource::Out, const int32 PerLoopIterations = -1)
		{
			TArray<FPCGPoint>& Points = PointIO->GetMutableData(Source)->GetMutablePoints();
			const int32 NumPoints = Points.Num();

			if (IsTrivial())
			{
				PrepareParallelLoopForPoints({PCGEx::H64(0, NumPoints)});
				for (int i = 0; i < NumPoints; i++) { ProcessSinglePoint(i, Points[i], 0, NumPoints); }
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);

			if (bInlineProcessPoints)
			{
				AsyncManagerPtr->Start<FAsyncProcessPointRangeInline<FPointsProcessor>>(0, nullptr, this, PLI, NumPoints, Source);
				return;
			}

			TArray<uint64> Loops;
			PCGExMT::SubRanges(Loops, NumPoints, PLI);

			PrepareParallelLoopForPoints(Loops);

			for (int i = 0; i < Loops.Num(); i++)
			{
				uint32 StartIndex;
				uint32 LoopIterations;
				PCGEx::H64(Loops[i], StartIndex, LoopIterations);
				AsyncManagerPtr->Start<FAsyncProcessPointRange<FPointsProcessor>>(StartIndex, nullptr, this, LoopIterations, Source, i);
			}
		}

		virtual void PrepareParallelLoopForPoints(const TArray<uint64>& Loops)
		{
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				PrepareParallelLoopForRange({PCGEx::H64(0, NumIterations)});
				for (int i = 0; i < NumIterations; i++) { ProcessSingleRangeIteration(i, 0, NumIterations); }
				return;
			}

			const int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);

			if (bInlineProcessPoints)
			{
				AsyncManagerPtr->Start<FAsyncProcessRangeInline<FPointsProcessor>>(0, nullptr, this, PLI, NumIterations);
				return;
			}

			TArray<uint64> Loops;
			PCGExMT::SubRanges(Loops, NumIterations, PLI);

			PrepareParallelLoopForRange(Loops);

			for (int i = 0; i < Loops.Num(); i++)
			{
				uint32 StartIndex;
				uint32 LoopIterations;
				PCGEx::H64(Loops[i], StartIndex, LoopIterations);
				AsyncManagerPtr->Start<FAsyncProcessRange<FPointsProcessor>>(StartIndex, nullptr, this, LoopIterations, PCGExData::ESource::Out, i);
			}
		}

		virtual void PrepareParallelLoopForRange(const TArray<uint64>& Loops)
		{
		}

		virtual void ProcessPoints(const PCGExData::ESource Source, const int32 StartIndex, const int32 Count, const int32 LoopIdx)
		{
			TArray<FPCGPoint>& Points = PointIO->GetMutableData(Source)->GetMutablePoints();
			for (int i = 0; i < Count; i++)
			{
				const int32 PtIndex = StartIndex + i;
				ProcessSinglePoint(PtIndex, Points[PtIndex], LoopIdx, Count);
			}
		}

		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
		{
		}

		virtual void ProcessRange(const int32 StartIndex, const int32 Iterations, const int32 LoopIdx)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Iterations); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}
	};

	class FPointsProcessorBatchBase
	{
	protected:
		PCGExMT::FTaskManager* AsyncManagerPtr = nullptr;
		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;

	public:
		bool bInlineProcessing = false;
		bool bInlineCompletion = false;
		bool bRequiresWriteStep = false;
		TArray<PCGExData::FFacade*> ProcessorFacades;

		mutable FRWLock BatchLock;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		FPCGContext* Context = nullptr;

		TArray<PCGExData::FPointIO*> PointsCollection;

		UPCGExOperation* PrimaryOperation = nullptr;

		FPointsProcessorBatchBase(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
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

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			PCGExMT::SubRanges(
				NumIterations, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations),
				[&](const int32 Index, const int32 Count)
				{
					AsyncManagerPtr->Start<FAsyncProcessRange<FPointsProcessorBatchBase>>(
						Index, nullptr, this, Count);
				});
		}

		void ProcessRange(const int32 StartIndex, const int32 Iterations, const int32 LoopIdx)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i, LoopIdx, Iterations); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
		{
		}
	};

	template <typename T>
	class TBatch : public FPointsProcessorBatchBase
	{
	public:
		TArray<T*> Processors;
		TArray<T*> ClosedBatchProcessors;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		TBatch(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			FPointsProcessorBatchBase(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
			ClosedBatchProcessors.Empty();
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

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				ProcessorFacades.Add(NewProcessor->PointDataFacade);

				if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
				if (PrimaryOperation) { NewProcessor->PrimaryOperation = PrimaryOperation; }

				NewProcessor->BatchIndex = Processors.Add(NewProcessor);

				if (IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize)
				{
					NewProcessor->bIsSmallPoints = true;
					ClosedBatchProcessors.Add(NewProcessor);
				}

				if (bInlineProcessing) { NewProcessor->bIsProcessorValid = NewProcessor->Process(AsyncManagerPtr); }
				else if (!NewProcessor->IsTrivial()) { AsyncManager->Start<FAsyncProcessWithUpdate<T>>(IO->IOIndex, IO, NewProcessor); }
			}

			StartClosedBatchProcessing();
		}

		virtual bool PrepareSingle(T* PointsProcessor)
		{
			return true;
		};

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;
			if (bInlineCompletion)
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->CompleteWork();
				}
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid || Processor->IsTrivial()) { continue; }
					AsyncManagerPtr->Start<FAsyncCompleteWork<T>>(-1, nullptr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		virtual void Write() override
		{
			CurrentState = PCGExMT::State_Writing;
			if (bInlineCompletion)
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->Write();
				}
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (!Processor->bIsProcessorValid || Processor->IsTrivial()) { continue; }
					PCGExMT::Write<T>(AsyncManagerPtr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations, const int32 LoopIdx)
		{
			if (CurrentState == PCGExMT::State_Processing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					Processor->bIsProcessorValid = Processor->Process(AsyncManagerPtr);
				}
			}
			else if (CurrentState == PCGExMT::State_Completing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->CompleteWork();
				}
			}
			else if (CurrentState == PCGExMT::State_Writing)
			{
				for (int i = 0; i < Iterations; i++)
				{
					T* Processor = ClosedBatchProcessors[StartIndex + i];
					if (!Processor->bIsProcessorValid) { continue; }
					Processor->Write();
				}
			}
		}

	protected:
		void StartClosedBatchProcessing()
		{
			const int32 NumTrivial = ClosedBatchProcessors.Num();
			if (NumTrivial > 0)
			{
				TArray<uint64> Loops;
				PCGExMT::SubRanges(Loops, ClosedBatchProcessors.Num(), GetDefault<UPCGExGlobalSettings>()->PointsDefaultBatchIterations);

				for (int i = 0; i < Loops.Num(); i++)
				{
					uint32 StartIndex;
					uint32 LoopIterations;
					PCGEx::H64(Loops[i], StartIndex, LoopIterations);
					AsyncManagerPtr->Start<FAsyncClosedBatchProcessRange<TBatch<T>>>(StartIndex, nullptr, this, LoopIterations, PCGExData::ESource::Out, i);
				}
			}
		}
	};

	static void ScheduleBatch(PCGExMT::FTaskManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		Manager->Start<FStartPointsBatchProcessing<FPointsProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void CompleteBatch(PCGExMT::FTaskManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		Manager->Start<FAsyncCompleteWork<FPointsProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void WriteBatch(PCGExMT::FTaskManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		PCGEX_ASYNC_WRITE(Manager, Batch)
	}
}


#undef PCGEX_POINTS_MT_TASK
#undef PCGEX_POINTS_MT_TASK_RANGE_INLINE
#undef PCGEX_POINTS_MT_TASK_RANGE
