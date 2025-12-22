// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Core/PCGExContext.h"

#define PCGEX_TYPED_PROCESSOR_NREF(_NAME) const TSharedRef<FProcessor> _NAME = StaticCastSharedRef<FProcessor>(InProcessor);
#define PCGEX_TYPED_PROCESSOR_REF PCGEX_TYPED_PROCESSOR_NREF(TypedProcessor)
#define PCGEX_TYPED_PROCESSOR const TSharedPtr<FProcessor> TypedProcessor = StaticCastSharedPtr<FProcessor>(InProcessor);

struct FPCGExContext;

namespace PCGExMT
{
	class FTaskManager;
	class FTaskGroup;
}

namespace PCGEx
{
	class FWorkHandle;
}

class UPCGSettings;
class UPCGExInstancedFactory;
class UPCGExPointFilterFactoryData;

namespace PCGExPointFilter
{
	class FManager;
}

namespace PCGExData
{
	class FPointIO;
	class FFacade;
	class FFacadePreloader;
}

namespace PCGExPointsMT
{
	PCGEX_CTX_STATE(MTState_PointsProcessing)
	PCGEX_CTX_STATE(MTState_PointsCompletingWork)
	PCGEX_CTX_STATE(MTState_PointsWriting)

#define PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, _BODY, _JIT)\
	PCGEX_CHECK_WORK_HANDLE_VOID\
	if (_INLINE_CONDITION)  { \
		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, _ID) \
		_ID->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS const TSharedRef<IProcessor>& Processor = This->Processors[Index]; _BODY }; \
		_JIT\
		_ID->StartIterations( Processors.Num(), 1, true);\
	} else {\
		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, _ID)\
		_ID->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS \
		const TSharedRef<IProcessor>& Processor = This->Processors[Index]; _BODY }; \
		_JIT\
		_ID->StartIterations(Processors.Num(), 1, false);\
	}

#define PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, _PLI) \
	PCGEX_CHECK_WORK_HANDLE_VOID\
	if (IsTrivial()){ TRACE_CPUPROFILER_EVENT_SCOPE(StartParallelLoopFor##_NAME##_Trivial) PCGExMT::FScope TrivialScope = PCGExMT::FScope(0, _NUM, 0); _PREPARE({TrivialScope}); _PROCESS(TrivialScope); _COMPLETE(); }else{\
	TRACE_CPUPROFILER_EVENT_SCOPE(StartParallelLoopFor##_NAME)\
	const int32 PLI = PCGEX_CORE_SETTINGS._PLI(PerLoopIterations); \
	PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, ParallelLoopFor##_NAME) \
	ParallelLoopFor##_NAME->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]() { PCGEX_ASYNC_THIS This->_COMPLETE(); }; \
	ParallelLoopFor##_NAME->OnPrepareSubLoopsCallback = [PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops) { PCGEX_ASYNC_THIS This->_PREPARE(Loops); }; \
	ParallelLoopFor##_NAME->OnSubLoopStartCallback =[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope) { PCGEX_ASYNC_THIS This->_PROCESS(Scope); }; \
    ParallelLoopFor##_NAME->StartSubLoops(_NUM, PLI, _INLINE);}

#define PCGEX_ASYNC_POINT_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE) PCGEX_ASYNC_PROCESSOR_LOOP(_NAME, _NUM, _PREPARE, _PROCESS, _COMPLETE, _INLINE, GetPointsBatchChunkSize)

#define PCGEX_ASYNC_MT_LOOP_VALID_PROCESSORS(_ID, _INLINE_CONDITION, _BODY, _JIT) PCGEX_ASYNC_MT_LOOP_TPL(_ID, _INLINE_CONDITION, if(Processor->bIsProcessorValid){ _BODY }, _JIT)

	class IBatch;

	class PCGEXFOUNDATIONS_API IProcessor : public TSharedFromThis<IProcessor>
	{
		friend class IBatch;

	protected:
		TSharedPtr<PCGExMT::FTaskManager> TaskManager;
		FPCGExContext* ExecutionContext = nullptr;
		UPCGSettings* ExecutionSettings = nullptr;

		TWeakPtr<PCGEx::FWorkHandle> WorkHandle;

		TSharedPtr<PCGExData::FFacadePreloader> InternalFacadePreloader;

		TSharedPtr<PCGExPointFilter::FManager> PrimaryFilters;
		bool bForceSingleThreadedProcessPoints = false;
		bool bForceSingleThreadedProcessRange = false;

		int32 LocalPointProcessingChunkSize = -1;

	public:
		TWeakPtr<IBatch> ParentBatch;
		TSharedPtr<PCGExMT::FTaskManager> GetTaskManager() { return TaskManager; }

		bool bIsProcessorValid = false;
		int32 BatchIndex = -1;
		bool bIsTrivial = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		TArray<int8> PointFilterCache;

		UPCGExInstancedFactory* PrimaryInstancedFactory = nullptr;

		template <typename T>
		T* GetPrimaryInstancedFactory() { return Cast<T>(PrimaryInstancedFactory); }

		explicit IProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		virtual void SetExecutionContext(FPCGExContext* InContext);

		virtual ~IProcessor() = default;

		virtual bool IsTrivial() const { return bIsTrivial; }

		bool HasFilters() const { return FilterFactories != nullptr; }
		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFactories);

		virtual void RegisterConsumableAttributesWithFacade() const;
		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader);
		void PrefetchData(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager, const TSharedPtr<PCGExMT::FTaskGroup>& InPrefetchDataTaskGroup);

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager);


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
		virtual bool InitPrimaryFilters(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories);
		virtual int32 FilterScope(const PCGExMT::FScope& Scope, const bool bParallel = false);
		virtual int32 FilterAll();
	};

	template <typename TContext, typename TSettings>
	class TProcessor : public IProcessor
	{
	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		explicit TProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: IProcessor(InPointDataFacade)
		{
		}

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			IProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
			check(Context)
			check(Settings)
		}

		TContext* GetContext() { return Context; }
		const TSettings* GetSettings() { return Settings; }
	};

	class PCGEXFOUNDATIONS_API IBatch : public TSharedFromThis<IBatch>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> TaskManager;
		TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* FilterFactories = nullptr;

		virtual TSharedPtr<IProcessor> NewProcessorInstance(const TSharedRef<PCGExData::FFacade>& InPointDataFacade) const;

	public:
		bool bPrefetchData = false;
		bool bForceSingleThreadedProcessing = false;
		bool bSkipCompletion = false;
		bool bForceSingleThreadedCompletion = false;
		bool bForceSingleThreadedWrite = false;
		bool bRequiresWriteStep = false;
		PCGExData::EIOInit DataInitializationPolicy = PCGExData::EIOInit::NoInit;
		TArray<TSharedRef<PCGExData::FFacade>> ProcessorFacades;
		TMap<PCGExData::FPointIO*, TSharedRef<IProcessor>>* SubProcessorMap = nullptr;

		mutable FRWLock BatchLock;

		std::atomic<PCGExCommon::ContextState> CurrentState{PCGExCommon::States::State_InitialExecution};

		FPCGExContext* ExecutionContext = nullptr;
		UPCGSettings* ExecutionSettings = nullptr;

		TWeakPtr<PCGEx::FWorkHandle> WorkHandle;

		TArray<TWeakPtr<PCGExData::FPointIO>> PointsCollection;

		UPCGExInstancedFactory* PrimaryInstancedFactory = nullptr;

		TArray<TSharedRef<IProcessor>> Processors;
		int32 GetNumProcessors() const { return Processors.Num(); }

		IBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection);
		virtual ~IBatch() = default;

		virtual void SetExecutionContext(FPCGExContext* InContext);

		template <typename T>
		T* GetContext() { return static_cast<T*>(ExecutionContext); }

		template <typename T>
		TSharedPtr<T> GetProcessor(const int32 Index) { return StaticCastSharedPtr<T>(Processors[Index].ToSharedPtr()); }

		template <typename T>
		TSharedRef<T> GetProcessorRef(const int32 Index) { return StaticCastSharedRef<T>(Processors[Index]); }

		void SetPointsFilterData(TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories) { FilterFactories = InFilterFactories; }

		virtual bool PrepareProcessing();
		virtual void Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager);

	protected:
		virtual void OnInitialPostProcess();

	public:
		virtual bool PrepareSingle(const TSharedRef<IProcessor>& InProcessor);
		virtual void CompleteWork();
		virtual void Write();
		virtual void Output();
		virtual void Cleanup();

	protected:
		virtual void OnProcessingPreparationComplete();
	};

	template <typename T>
	class TBatch : public IBatch
	{
	protected:
		virtual TSharedPtr<IProcessor> NewProcessorInstance(const TSharedRef<PCGExData::FFacade>& InPointDataFacade) const override
		{
			TSharedPtr<IProcessor> NewInstance = MakeShared<T>(InPointDataFacade);
			return NewInstance;
		}

	public:
		TBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
			: IBatch(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
		}
	};

	PCGEXFOUNDATIONS_API void ScheduleBatch(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<IBatch>& Batch);
}
