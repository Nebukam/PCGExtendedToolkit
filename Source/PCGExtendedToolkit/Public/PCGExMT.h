// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include <atomic>

#include "PCGContext.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMacros.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGAsync.h"

#pragma region MT MACROS

#define PCGEX_ASYNC_WRITE(_MANAGER, _TARGET) if(_TARGET){ PCGExMT::Write(_MANAGER, _TARGET); }
#define PCGEX_ASYNC_WRITE_DELETE(_MANAGER, _TARGET) if(_TARGET){ PCGExMT::WriteAndDelete(_MANAGER, _TARGET); _TARGET = nullptr; }

#pragma endregion

namespace PCGExMT
{
	static void SetWorkPriority(EPCGExAsyncPriority Selection, EQueuedWorkPriority& Priority)
	{
		switch (Selection)
		{
		case EPCGExAsyncPriority::Blocking:
			Priority = EQueuedWorkPriority::Blocking;
			break;
		case EPCGExAsyncPriority::Highest:
			Priority = EQueuedWorkPriority::Highest;
			break;
		case EPCGExAsyncPriority::High:
			Priority = EQueuedWorkPriority::High;
			break;
		default:
		case EPCGExAsyncPriority::Normal:
			Priority = EQueuedWorkPriority::Normal;
			break;
		case EPCGExAsyncPriority::Low:
			Priority = EQueuedWorkPriority::Low;
			break;
		case EPCGExAsyncPriority::Lowest:
			Priority = EQueuedWorkPriority::Lowest;
			break;
		case EPCGExAsyncPriority::Default:
			SetWorkPriority(GetDefault<UPCGExGlobalSettings>()->GetDefaultWorkPriority(), Priority);
			break;
		}
	}

#define PCGEX_ASYNC_STATE(_NAME) const PCGExMT::AsyncState _NAME = GetTypeHash(FName(#_NAME));

	constexpr int32 GAsyncLoop_XS = 32;
	constexpr int32 GAsyncLoop_S = 64;
	constexpr int32 GAsyncLoop_M = 256;
	constexpr int32 GAsyncLoop_L = 512;
	constexpr int32 GAsyncLoop_XL = 1024;

	using AsyncState = uint64;

	PCGEX_ASYNC_STATE(State_Setup)
	PCGEX_ASYNC_STATE(State_ReadyForNextPoints)
	PCGEX_ASYNC_STATE(State_ProcessingPoints)

	PCGEX_ASYNC_STATE(State_ProcessingTargets)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncWork)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncProcessing)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncCompletion)
	PCGEX_ASYNC_STATE(State_Done)

	PCGEX_ASYNC_STATE(State_Processing)
	PCGEX_ASYNC_STATE(State_Completing)
	PCGEX_ASYNC_STATE(State_Writing)

	PCGEX_ASYNC_STATE(State_CompoundWriting)
	PCGEX_ASYNC_STATE(State_MetaWriting)
	PCGEX_ASYNC_STATE(State_MetaWriting2)

	template <class ChunkFunc>
	static void SubRanges(const int32 MaxItems, const int32 RangeSize, ChunkFunc&& Func)
	{
		int32 CurrentCount = 0;
		while (CurrentCount < MaxItems)
		{
			Func(CurrentCount, FMath::Min(MaxItems - CurrentCount, RangeSize));
			CurrentCount += RangeSize;
		}
	}

	static int32 SubRanges(TArray<uint64>& OutSubRanges, const int32 MaxItems, const int32 RangeSize)
	{
		OutSubRanges.Empty();
		OutSubRanges.Reserve(MaxItems / RangeSize + 1);
		int32 CurrentCount = 0;
		while (CurrentCount < MaxItems)
		{
			OutSubRanges.Add(PCGEx::H64(CurrentCount, FMath::Min(MaxItems - CurrentCount, RangeSize)));
			CurrentCount += RangeSize;
		}
		return OutSubRanges.Num();
	}

	struct PCGEXTENDEDTOOLKIT_API FAsyncParallelLoop
	{
		FAsyncParallelLoop()
		{
		}

		FAsyncParallelLoop(FPCGContext* InContext, const int32 InChunkSize, const bool InEnabled):
			Context(InContext), ChunkSize(InChunkSize), bAsyncEnabled(InEnabled)
		{
		}

		FPCGContext* Context = nullptr;
		int32 ChunkSize = 32;
		bool bAsyncEnabled = true;
		int32 NumIterations = -1;

		int32 CurrentIndex = -1;

		template <class InitializeFunc, class LoopBodyFunc>
		bool Execute(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody)
		{
			if (bAsyncEnabled)
			{
				return FPCGAsync::AsyncProcessingOneToOneEx(
					&(Context->AsyncState), NumIterations, Initialize, [&](int32 ReadIndex, int32 WriteIndex)
					{
						LoopBody(ReadIndex);
						return true;
					}, true, ChunkSize);
			}

			if (CurrentIndex == -1)
			{
				Initialize();
				CurrentIndex = 0;
			}

			const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
			if (ChunkNumIterations <= 0)
			{
				CurrentIndex = -1;
				return true;
			}
			for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i); }
			CurrentIndex += ChunkNumIterations;
			return false;
		}

		template <class LoopBodyFunc>
		bool Execute(LoopBodyFunc&& LoopBody)
		{
			if (bAsyncEnabled)
			{
				return FPCGAsync::AsyncProcessingOneToOneEx(
					&(Context->AsyncState), NumIterations, []()
					{
					}, [&](int32 ReadIndex, int32 WriteIndex)
					{
						LoopBody(ReadIndex);
						return true;
					}, true, ChunkSize);
			}

			if (CurrentIndex == -1) { CurrentIndex = 0; }
			const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
			if (ChunkNumIterations <= 0)
			{
				CurrentIndex = -1;
				return true;
			}
			for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i); }
			CurrentIndex += ChunkNumIterations;
			return false;
		}

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	class FPCGExTask;
	class FTaskGroup;
	class FGroupRangeCallbackTask;

	class PCGEXTENDEDTOOLKIT_API FTaskManager
	{
		friend class FPCGExTask;
		friend class FTaskGroup;

	public:
		~FTaskManager();

		EQueuedWorkPriority WorkPriority = EQueuedWorkPriority::Normal;

		mutable FRWLock ManagerLock;
		FPCGContext* Context;
		std::atomic<bool> Stopped = false;
		std::atomic<bool> ForceSync = false;

		FTaskGroup* CreateGroup();

		FORCEINLINE bool IsAvailable() const { return Stopped.load() || Flushing.load() ? false : true; }

		template <typename T, typename... Args>
		void Start(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			if (!IsAvailable()) { return; }
			if (ForceSync) { StartSynchronousTask<T>(new FAsyncTask<T>(InPointsIO, args...), TaskIndex); }
			else { StartBackgroundTask<T>(new FAsyncTask<T>(InPointsIO, args...), TaskIndex); }
		}

		template <typename T, typename... Args>
		void StartSynchronous(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			if (!IsAvailable()) { return; }
			StartSynchronousTask(new FAsyncTask<T>(InPointsIO, args...), TaskIndex);
		}

		template <typename T>
		void StartBackgroundTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
		{
			if (!IsAvailable()) { return; }
			++NumStarted;

			{
				FWriteScopeLock WriteLock(ManagerLock);
				QueuedTasks.Add(AsyncTask);
			}

			T& Task = AsyncTask->GetTask();
			//Task.TaskPtr = AsyncTask;
			Task.Manager = this;
			Task.TaskIndex = TaskIndex;

			AsyncTask->StartBackgroundTask(GThreadPool, WorkPriority);
		}

		template <typename T>
		void StartSynchronousTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
		{
			if (!IsAvailable()) { return; }

			T& Task = AsyncTask->GetTask();
			//Task.TaskPtr = AsyncTask;
			Task.Manager = this;
			Task.TaskIndex = TaskIndex;
			Task.bIsAsync = false;

			Task.ExecuteTask();
			delete AsyncTask;
		}

		void Reserve(const int32 NumTasks)
		{
			FWriteScopeLock WriteLock(ManagerLock);
			QueuedTasks.Reserve(NumTasks);
		}

		void OnAsyncTaskExecutionComplete(FPCGExTask* AsyncTask, bool bSuccess);
		bool IsAsyncWorkComplete() const;

		void Reset();

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

	protected:
		std::atomic<bool> Flushing = {false};
		std::atomic<int32> NumStarted = {0};
		std::atomic<int32> NumCompleted = {0};
		TArray<FAsyncTaskBase*> QueuedTasks;
		TArray<FTaskGroup*> Groups;
	};

	class PCGEXTENDEDTOOLKIT_API FTaskGroup
	{
		friend class FTaskManager;

		friend class FPCGExTask;
		friend class FGroupRangeCallbackTask;
		friend class FGroupPrepareRangeTask;
		friend class FGroupRangeIterationTask;
		friend class FGroupRangeInlineIterationTask;

	public:
		using CompletionCallback = std::function<void()>;
		using IterationCallback = std::function<void(const int32, const int32, const int32)>;
		using IterationRangeStartCallback = std::function<void(const int32, const int32, const int32)>;

		explicit FTaskGroup(FTaskManager* InManager):
			Manager(InManager)
		{
		}

		~FTaskGroup()
		{
		}

		void SetOnCompleteCallback(const CompletionCallback& Callback)
		{
			bHasOnCompleteCallback = true;
			OnCompleteCallback = Callback;
		}

		void SetOnIterationRangeStartCallback(const IterationRangeStartCallback& Callback)
		{
			bHasOnIterationRangeStartCallback = true;
			OnIterationRangeStartCallback = Callback;
		}

		template <typename T, typename... Args>
		void Start(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			if (!Manager->IsAvailable()) { return; }

			++NumStarted;
			FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, args...);
			ATask->GetTask().Group = this;
			if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, TaskIndex); }
			else { Manager->StartBackgroundTask<T>(ATask, TaskIndex); }
		}

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			if (!Manager->IsAvailable()) { return; }

			TArray<uint64> Loops;
			NumStarted += SubRanges(Loops, MaxItems, ChunkSize);

			int32 LoopIdx = 0;
			for (const uint64 H : Loops)
			{
				FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, args...);
				ATask->GetTask().Group = this;
				ATask->GetTask().Scope = H;

				if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, LoopIdx++); }
				else { Manager->StartBackgroundTask<T>(ATask, LoopIdx++); }
			}
		}

		void StartRanges(const IterationCallback& Callback, const int32 MaxItems, const int32 ChunkSize, const bool bInlined = false, const bool bExecuteSmallSynchronously = true);

		void PrepareRangesOnly(const int32 MaxItems, const int32 ChunkSize);

	protected:
		FTaskManager* Manager;

		mutable FRWLock GroupLock;

		bool bHasOnCompleteCallback = false;
		CompletionCallback OnCompleteCallback;

		IterationCallback OnIterationCallback;
		bool bHasOnIterationRangeStartCallback = false;
		IterationRangeStartCallback OnIterationRangeStartCallback;

		std::atomic<int> NumStarted = {0};
		std::atomic<int> NumCompleted = {0};

		void PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;
		void DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;
		void InternalStartInlineRange(const int32 Index, const int32 MaxItems, const int32 ChunkSize);

		void OnTaskCompleted();

	};

	class PCGEXTENDEDTOOLKIT_API PCGExMT::FPCGExTask : public FNonAbandonableTask
	{
		friend class FTaskManager;
		friend class FTaskGroup;

	protected:
		bool bIsAsync = true;

	public:
		virtual ~FPCGExTask()
		{
			Manager = nullptr;
			Group = nullptr;
		}

		FTaskManager* Manager = nullptr;
		FTaskGroup* Group = nullptr;
		int32 TaskIndex = -1;
		//FAsyncTaskBase* TaskPtr = nullptr;
		PCGExData::FPointIO* PointIO = nullptr;

#define PCGEX_ASYNC_CHECKPOINT_VOID  if (!Checkpoint()) { return; }
#define PCGEX_ASYNC_CHECKPOINT  if (!Checkpoint()) { return false; }

		PCGExMT::FPCGExTask(PCGExData::FPointIO* InPointIO) : PointIO(InPointIO)
		{
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPCGExAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork()
		{
			if (bWorkDone) { return; }
			PCGEX_ASYNC_CHECKPOINT_VOID
			bWorkDone = true;
			const bool bResult = ExecuteTask();
			if (Group) { Group->OnTaskCompleted(); }
			Manager->OnAsyncTaskExecutionComplete(this, bResult);
		}

		virtual bool ExecuteTask() = 0;

	protected:
		bool bWorkDone = false;
		bool Checkpoint() const { return !(!Manager || !Manager->IsAvailable()); }

		template <typename T, typename... Args>
		void InternalStart(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			PCGEX_ASYNC_CHECKPOINT_VOID
			if (!bIsAsync) { Manager->StartSynchronous<T>(TaskIndex, InPointsIO, args...); }
			else { Manager->Start<T>(TaskIndex, InPointsIO, args...); }
		}

		template <typename T, typename... Args>
		void InternalStartSync(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
		{
			PCGEX_ASYNC_CHECKPOINT_VOID
			Manager->StartSynchronous<T>(TaskIndex, InPointsIO, args...);
		}
	};

	class FGroupRangeCallbackTask : public FPCGExTask
	{
	public:
		FGroupRangeCallbackTask(PCGExData::FPointIO* InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 NumIterations = 0;
		virtual bool ExecuteTask() override;
	};

	class FGroupRangeIterationTask : public FPCGExTask
	{
	public:
		FGroupRangeIterationTask(PCGExData::FPointIO* InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask() override;
	};

	class FGroupPrepareRangeTask : public FPCGExTask
	{
	public:
		FGroupPrepareRangeTask(PCGExData::FPointIO* InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask() override;
	};

	class FGroupRangeInlineIterationTask : public FPCGExTask
	{
	public:
		FGroupRangeInlineIterationTask(PCGExData::FPointIO* InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 MaxItems = 0;
		int32 ChunkSize = 0;
		virtual bool ExecuteTask() override;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FWriteTask final : public FPCGExTask
	{
	public:
		FWriteTask(PCGExData::FPointIO* InPointIO,
		           T* InOperation)
			: FPCGExTask(InPointIO),
			  Operation(InOperation)

		{
		}

		T* Operation = nullptr;

		virtual bool ExecuteTask() override
		{
			Operation->Write();
			return false;
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FWriteAndDeleteTask final : public FPCGExTask
	{
	public:
		FWriteAndDeleteTask(PCGExData::FPointIO* InPointIO,
		                    T* InOperation)
			: FPCGExTask(InPointIO),
			  Operation(InOperation)

		{
		}

		T* Operation = nullptr;

		virtual bool ExecuteTask() override
		{
			Operation->Write();
			PCGEX_DELETE(Operation)
			return false;
		}
	};

	template <typename T>
	static void Write(FTaskManager* AsyncManager, T* Operation) { AsyncManager->Start<FWriteTask<T>>(-1, nullptr, Operation); }

	template <typename T>
	static void WriteAndDelete(FTaskManager* AsyncManager, T* Operation) { AsyncManager->Start<FWriteAndDeleteTask<T>>(-1, nullptr, Operation); }
}
