// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"

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
	PCGEX_CHECK_WORK_PERMIT_VOID\
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
	PCGEX_CHECK_WORK_PERMIT_VOID\
	if (IsTrivial()){ _PREPARE({PCGExMT::FScope(0, _NUM, 0)}); _PROCESS(PCGExMT::FScope(0, _NUM, 0)); _COMPLETE(); return; } \
	const int32 PLI = GetDefault<UPCGExGlobalSettings>()->_PLI(PerLoopIterations); \
	PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, ParallelLoopFor##_NAME) \
	ParallelLoopFor##_NAME->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]() { PCGEX_ASYNC_THIS This->_COMPLETE(); }; \
	ParallelLoopFor##_NAME->OnPrepareSubLoopsCallback = [PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops) { PCGEX_ASYNC_THIS This->_PREPARE(Loops); }; \
	ParallelLoopFor##_NAME->OnSubLoopStartCallback =[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS This->_PROCESS(Scope); }; \
    ParallelLoopFor##_NAME->StartSubLoops(_NUM, PLI, _INLINE);

#define PCGEX_ASYNC_POINT_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetPointsBatchChunkSize)

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY })

#define PCGEX_ASYNC_CLUSTER_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetClusterBatchChunkSize)

#pragma region Tasks

	template <typename T>
	class FStartBatchProcessing final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FStartClusterBatchProcessing)

		FStartBatchProcessing(TSharedPtr<T> InTarget)
			: FTask(),
			  Target(InTarget)
		{
		}

		TSharedPtr<T> Target;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			Target->Process(AsyncManager);
		}
	};

#pragma endregion

	class IPointsProcessorBatch;

	class PCGEXTENDEDTOOLKIT_API FPointsProcessor : public TSharedFromThis<FPointsProcessor>
	{
		friend class IPointsProcessorBatch;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		FPCGExContext* ExecutionContext = nullptr;
		TWeakPtr<PCGEx::FWorkPermit> WorkPermit;

		TSharedPtr<PCGExData::FFacadePreloader> InternalFacadePreloader;

		TSharedPtr<PCGExPointFilter::FManager> PrimaryFilters;
		bool bDaisyChainProcessPoints = false;
		bool bDaisyChainProcessRange = false;

		int32 LocalPointProcessingChunkSize = -1;

	public:
		TWeakPtr<IPointsProcessorBatch> ParentBatch;
		TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager() { return AsyncManager; }

		bool bIsProcessorValid = false;
		int32 BatchIndex = -1;
		bool bIsTrivial = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TArray<TObjectPtr<const UPCGExFilterFactoryData>>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		TArray<int8> PointFilterCache;

		UPCGExInstancedFactory* PrimaryInstancedFactory = nullptr;

		template <typename T>
		T* GetPrimaryInstancedFactory() { return Cast<T>(PrimaryInstancedFactory); }

		explicit FPointsProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		virtual void SetExecutionContext(FPCGExContext* InContext);

		virtual ~FPointsProcessor() = default;

		virtual bool IsTrivial() const { return bIsTrivial; }

		bool HasFilters() const { return FilterFactories != nullptr; }

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFactories);

		virtual void RegisterConsumableAttributesWithFacade() const;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader);

		void PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup);

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager);


#pragma region Parallel loop for points

		void StartParallelLoopForPoints(const PCGExData::EIOSide Side = PCGExData::EIOSide::Out, const int32 PerLoopIterations = -1);
		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops);
		virtual void ProcessPoints(const PCGExMT::FScope& Scope);
		virtual void OnPointsProcessingComplete();

#pragma endregion

#pragma region Parallel loop for Range

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1);
		virtual void PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops);
		virtual void ProcessRange(const PCGExMT::FScope& Scope);
		virtual void OnRangeProcessingComplete();

#pragma endregion

		virtual void CompleteWork();
		virtual void Write();
		virtual void Output();
		virtual void Cleanup();

	protected:
		virtual bool InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories);
		virtual int32 FilterScope(const PCGExMT::FScope& Scope);
		virtual int32 FilterAll();
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

		TContext* GetContext() { return Context; }
		const TSettings* GetSettings() { return Settings; }
	};

	class PCGEXTENDEDTOOLKIT_API IPointsProcessorBatch : public TSharedFromThis<IPointsProcessorBatch>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> AsyncManager;
		TArray<TObjectPtr<const UPCGExFilterFactoryData>>* FilterFactories = nullptr;

	public:
		bool bPrefetchData = false;
		bool bDaisyChainProcessing = false;
		bool bSkipCompletion = false;
		bool bDaisyChainCompletion = false;
		bool bDaisyChainWrite = false;
		bool bRequiresWriteStep = false;
		TArray<TSharedRef<PCGExData::FFacade>> ProcessorFacades;
		TMap<PCGExData::FPointIO*, TSharedRef<FPointsProcessor>>* SubProcessorMap = nullptr;

		mutable FRWLock BatchLock;

		std::atomic<PCGEx::ContextState> CurrentState{PCGEx::State_InitialExecution};

		FPCGExContext* ExecutionContext = nullptr;
		TWeakPtr<PCGEx::FWorkPermit> WorkPermit;

		TArray<TWeakPtr<PCGExData::FPointIO>> PointsCollection;

		UPCGExInstancedFactory* PrimaryInstancedFactory = nullptr;

		virtual int32 GetNumProcessors() const { return -1; }

		IPointsProcessorBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection);

		virtual ~IPointsProcessorBatch() = default;

		virtual void SetExecutionContext(FPCGExContext* InContext);

		template <typename T>
		T* GetContext() { return static_cast<T*>(ExecutionContext); }

		virtual bool PrepareProcessing();

		virtual void Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager);

		virtual void CompleteWork();
		virtual void Write();
		virtual void Output();
		virtual void Cleanup();
	};

	template <typename T>
	class TBatch : public IPointsProcessorBatch
	{
	public:
		TArray<TSharedRef<T>> Processors;
		TArray<TSharedRef<T>> TrivialProcessors;

		virtual int32 GetNumProcessors() const override { return Processors.Num(); }

		std::atomic<PCGEx::ContextState> CurrentState{PCGEx::State_InitialExecution};

		TBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection):
			IPointsProcessorBatch(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
		}

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExFilterFactoryData>>* InFilterFactories)
		{
			FilterFactories = InFilterFactories;
		}

		virtual bool PrepareProcessing() override
		{
			return IPointsProcessorBatch::PrepareProcessing();
		}

		virtual void Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override
		{
			if (PointsCollection.IsEmpty()) { return; }

			CurrentState.store(PCGEx::State_Processing, std::memory_order_release);

			AsyncManager = InAsyncManager;
			PCGEX_ASYNC_CHKD_VOID(AsyncManager)

			TSharedPtr<IPointsProcessorBatch> SelfPtr = SharedThis(this);

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
				if (PrimaryInstancedFactory) { NewProcessor->PrimaryInstancedFactory = PrimaryInstancedFactory; }

				NewProcessor->RegisterConsumableAttributesWithFacade();

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

				ParallelAttributeRead->OnIterationCallback =
					[PCGEX_ASYNC_THIS_CAPTURE, ParallelAttributeRead](const int32 Index, const PCGExMT::FScope& Scope)
					{
						PCGEX_ASYNC_THIS
						This->Processors[Index]->PrefetchData(This->AsyncManager, ParallelAttributeRead);
					};

				ParallelAttributeRead->StartIterations(Processors.Num(), 1);
			}
			else
			{
				OnProcessingPreparationComplete();
			}
		}

	protected:
		virtual void OnProcessingPreparationComplete()
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
			if (bSkipCompletion) { return; }
			CurrentState.store(PCGEx::State_Completing, std::memory_order_release);
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(CompleteWork, bDaisyChainCompletion, { Processor->CompleteWork(); })
			IPointsProcessorBatch::CompleteWork();
		}

		virtual void Write() override
		{
			CurrentState.store(PCGEx::State_Writing, std::memory_order_release);
			PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(Write, bDaisyChainWrite, { Processor->Write(); })
			IPointsProcessorBatch::Write();
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
			IPointsProcessorBatch::Cleanup();
			for (const TSharedPtr<T>& P : Processors) { P->Cleanup(); }
			Processors.Empty();
		}
	};

	static void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<IPointsProcessorBatch>& Batch)
	{
		PCGEX_LAUNCH(FStartBatchProcessing<IPointsProcessorBatch>, Batch)
	}
}
