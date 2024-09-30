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
#define PCGEX_ASYNC_GROUP_CHKD_VOID(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return; }
#define PCGEX_ASYNC_GROUP_CHKD(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return false; }

#pragma endregion

namespace PCGExMT
{
	static void SetWorkPriority(const EPCGExAsyncPriority Selection, EQueuedWorkPriority& Priority)
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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FAsyncParallelLoop
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
			for (int i = 0; i < ChunkNumIterations; ++i) { LoopBody(CurrentIndex + i); }
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
			for (int i = 0; i < ChunkNumIterations; ++i) { LoopBody(CurrentIndex + i); }
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

	class /*PCGEXTENDEDTOOLKIT_API*/ FTaskManager : public TSharedFromThis<FTaskManager>
	{
		friend class FPCGExTask;
		friend class FPCGExDeferredUnpauseTask;
		friend class FTaskGroup;

	public:
		FTaskManager(FPCGExContext* InContext)
			: Context(InContext)
		{
			PCGEX_LOG_CTR(FTaskManager)
		}

		~FTaskManager();

		EQueuedWorkPriority WorkPriority = EQueuedWorkPriority::Normal;

		mutable FRWLock ManagerLock;
		mutable FRWLock PauseLock;
		mutable FRWLock GroupLock;

		FPCGExContext* Context = nullptr;

		int8 Stopped = 0;
		int8 ForceSync = 0;

		void GrowNumStarted();
		void GrowNumCompleted();

		TSharedPtr<FTaskGroup> TryCreateGroup(const FName& GroupName);

		FORCEINLINE bool IsAvailable() const { return Stopped ? false : true; }

		template <typename T, typename... Args>
		void Start(const int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			if (!IsAvailable()) { return; }

			FAsyncTask<T>* UniqueTask = new FAsyncTask<T>(InPointsIO, args...);
			if (ForceSync) { StartSynchronousTask<T>(UniqueTask, TaskIndex); }
			else { StartBackgroundTask<T>(UniqueTask, TaskIndex); }
		}

		template <typename T, typename... Args>
		void StartSynchronous(int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			if (!IsAvailable()) { return; }

			FAsyncTask<T>* UniqueTask = new FAsyncTask<T>(InPointsIO, args...);
			StartSynchronousTask(UniqueTask, TaskIndex);
		}

		template <typename T>
		void StartBackgroundTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
		{
			if (!IsAvailable()) { return; }
			GrowNumStarted();

			{
				FWriteScopeLock WriteLock(ManagerLock);
				QueuedTasks.Add(TUniquePtr<FAsyncTaskBase>(AsyncTask));
			}

			T& Task = AsyncTask->GetTask();
			//Task.TaskPtr = AsyncTask;
			Task.ManagerPtr = SharedThis(this);
			Task.TaskIndex = TaskIndex;

			AsyncTask->StartBackgroundTask(GThreadPool, WorkPriority);
		}

		template <typename T>
		void StartSynchronousTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
		{
			if (!IsAvailable()) { return; }
			GrowNumStarted();

			T& Task = AsyncTask->GetTask();
			//Task.TaskPtr = AsyncTask;
			Task.ManagerPtr = SharedThis(this);
			Task.TaskIndex = TaskIndex;
			Task.bIsAsync = false;

			Task.DoWork();
			delete AsyncTask;
		}

		void Reserve(const int32 NumTasks)
		{
			FWriteScopeLock WriteLock(ManagerLock);
			QueuedTasks.Reserve(QueuedTasks.Num() + NumTasks);
		}

		bool IsWorkComplete() const;

		void Reset(const bool bHoldStop = false);

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

	protected:
		int32 NumStarted = 0;
		int32 NumCompleted = 0;
		TArray<TUniquePtr<FAsyncTaskBase>> QueuedTasks;
		TArray<TSharedPtr<FTaskGroup>> Groups;

		int8 CompletionScheduled = 0;
		int8 WorkComplete = 1;
		void ScheduleCompletion();
		void TryComplete();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTaskGroup : public TSharedFromThis<FTaskGroup>
	{
		friend class FTaskManager;

		friend class FPCGExTask;
		friend class FGroupRangeCallbackTask;
		friend class FGroupPrepareRangeTask;
		friend class FGroupRangeIterationTask;
		friend class FGroupPrepareRangeInlineTask;
		friend class FGroupRangeInlineIterationTask;

		FName GroupName = NAME_None;

	public:
		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		using IterationCallback = std::function<void(const int32, const int32, const int32)>;
		IterationCallback OnIterationCallback;

		using IterationRangePrepareCallback = std::function<void(const TArray<uint64>&)>;
		IterationRangePrepareCallback OnIterationRangePrepareCallback;

		using IterationRangeStartCallback = std::function<void(const int32, const int32, const int32)>;
		IterationRangeStartCallback OnIterationRangeStartCallback;

		explicit FTaskGroup(const TSharedPtr<FTaskManager>& InManager, const FName InGroupName):
			GroupName(InGroupName), Manager(InManager)
		{
			InManager->GrowNumStarted();
		}

		~FTaskGroup()
		{
		}

		template <typename T, typename... Args>
		void Start(const int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			if (!Manager->IsAvailable()) { return; }

			FPlatformAtomics::InterlockedAdd(&NumStarted, 1);
			FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, args...);
			ATask->GetTask().GroupPtr = SharedThis(this);
			if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, TaskIndex); }
			else { Manager->StartBackgroundTask<T>(ATask, TaskIndex); }
		}

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			if (!Manager->IsAvailable()) { return; }

			TArray<uint64> Loops;
			FPlatformAtomics::InterlockedAdd(&NumStarted, SubRanges(Loops, MaxItems, ChunkSize));

			if (OnIterationRangePrepareCallback) { OnIterationRangePrepareCallback(Loops); }

			int32 LoopIdx = 0;
			for (const uint64 H : Loops)
			{
				FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, args...);
				ATask->GetTask().GroupPtr = SharedThis(this);
				ATask->GetTask().Scope = H;

				if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, LoopIdx++); }
				else { Manager->StartBackgroundTask<T>(ATask, LoopIdx++); }
			}
		}

		void StartRanges(const IterationCallback& Callback, const int32 MaxItems, const int32 ChunkSize, const bool bInlined = false, const bool bExecuteSmallSynchronously = true);

		void PrepareRangesOnly(const int32 MaxItems, const int32 ChunkSize, const bool bInline = false);

	protected:
		TSharedPtr<FTaskManager> Manager;

		mutable FRWLock GroupLock;

		int32 NumStarted = 0;
		int32 NumCompleted = 0;

		void PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;
		void DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;

		template <typename T>
		void InternalStartInlineRange(const int32 Index, const int32 MaxItems, const int32 ChunkSize)
		{
			FAsyncTask<T>* NextRange = new FAsyncTask<T>(nullptr);
			NextRange->GetTask().GroupPtr = SharedThis(this);
			NextRange->GetTask().MaxItems = MaxItems;
			NextRange->GetTask().ChunkSize = FMath::Max(1, ChunkSize);
			

			if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(NextRange, Index); }
			else { Manager->StartBackgroundTask<T>(NextRange, Index); }
		}


		void OnTaskCompleted();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTask : public FNonAbandonableTask
	{
		friend class FTaskManager;
		friend class FTaskGroup;

	protected:
		bool bIsAsync = true;

	public:
		virtual ~FPCGExTask() = default;

		TWeakPtr<FTaskManager> ManagerPtr;
		TWeakPtr<FTaskGroup> GroupPtr;
		int32 TaskIndex = -1;
		//FAsyncTaskBase* TaskPtr = nullptr;
		TSharedPtr<PCGExData::FPointIO> PointIO;

		FPCGExTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
			: PointIO(InPointIO)
		{
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPCGExAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		void DoWork();
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) = 0;

	protected:
		bool bWorkDone = false;

		template <typename T, typename... Args>
		void InternalStart(int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			const TSharedPtr<FTaskManager> Manager = ManagerPtr.Pin();
			if (!Manager || !Manager->IsAvailable()) { return; }
			if (!bIsAsync) { Manager->StartSynchronous<T>(InTaskIndex, InPointsIO, args...); }
			else { Manager->Start<T>(InTaskIndex, InPointsIO, args...); }
		}

		template <typename T, typename... Args>
		void InternalStartSync(int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args... args)
		{
			const TSharedPtr<FTaskManager> Manager = ManagerPtr.Pin();
			if (!Manager || !Manager->IsAvailable()) { return; }
			Manager->StartSynchronous<T>(InTaskIndex, InPointsIO, args...);
		}
	};

	class FGroupRangeCallbackTask : public FPCGExTask
	{
	public:
		explicit FGroupRangeCallbackTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 NumIterations = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupRangeIterationTask : public FPCGExTask
	{
	public:
		explicit FGroupRangeIterationTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupPrepareRangeTask : public FPCGExTask
	{
	public:
		explicit FGroupPrepareRangeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupPrepareRangeInlineTask : public FPCGExTask
	{
	public:
		explicit FGroupPrepareRangeInlineTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 MaxItems = 0;
		int32 ChunkSize = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupRangeInlineIterationTask : public FPCGExTask
	{
	public:
		explicit FGroupRangeInlineIterationTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 MaxItems = 0;
		int32 ChunkSize = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTask final : public FPCGExTask
	{
	public:
		FWriteTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		           const TSharedPtr<T>& InOperation)
			: FPCGExTask(InPointIO),
			  Operation(InOperation)

		{
		}

		TSharedPtr<T> Operation;

		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override
		{
			if (!Operation) { return false; }
			Operation->Write();
			return true;
		}
	};

	template <typename T>
	static void Write(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<T>& Operation)
	{
		if (!Operation) { return; }
		if (!AsyncManager || !AsyncManager->IsAvailable()) { Operation->Write(); }
		else { AsyncManager->Start<FWriteTask<T>>(-1, nullptr, Operation); }
	}
}
