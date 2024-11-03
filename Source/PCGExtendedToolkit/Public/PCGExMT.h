// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include <atomic>
#include "CoreMinimal.h"

#include "Misc/ScopeRWLock.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"
#include "Async/AsyncWork.h"
#include "Misc/QueuedThreadPool.h"

#include "PCGExMacros.h"
#include "PCGExGlobalSettings.h"
#include "Data/PCGExPointIO.h"

#pragma region MT MACROS

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

	constexpr int32 GAsyncLoop_XS = 32;
	constexpr int32 GAsyncLoop_S = 64;
	constexpr int32 GAsyncLoop_M = 256;
	constexpr int32 GAsyncLoop_L = 512;
	constexpr int32 GAsyncLoop_XL = 1024;

	static int32 SubRanges(TArray<uint64>& OutSubRanges, const int32 MaxItems, const int32 RangeSize)
	{
		OutSubRanges.Empty();
		OutSubRanges.Reserve((MaxItems + RangeSize - 1) / RangeSize);

		for (int32 CurrentCount = 0; CurrentCount < MaxItems; CurrentCount += RangeSize)
		{
			OutSubRanges.Add(PCGEx::H64(CurrentCount, FMath::Min(RangeSize, MaxItems - CurrentCount)));
		}

		return OutSubRanges.Num();
	}

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
		void Start(const int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			FAsyncTask<T>* UniqueTask = new FAsyncTask<T>(InPointsIO, std::forward<Args>(InArgs)...);
			if (ForceSync) { StartSynchronousTask<T>(UniqueTask, TaskIndex); }
			else { StartBackgroundTask<T>(UniqueTask, TaskIndex); }
		}

		template <typename T, typename... Args>
		void StartSynchronous(int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			FAsyncTask<T>* UniqueTask = new FAsyncTask<T>(InPointsIO, std::forward<Args>(InArgs)...);
			StartSynchronousTask(UniqueTask, TaskIndex);
		}

		template <typename T>
		void StartBackgroundTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
		{
			if (!IsAvailable())
			{
				delete AsyncTask;
				return;
			}

			GrowNumStarted();

			{
				FWriteScopeLock WriteLock(ManagerLock);
				QueuedTasks.Add(AsyncTask);
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
			if (!IsAvailable())
			{
				delete AsyncTask;
				return;
			}

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
		TArray<FAsyncTaskBase*> QueuedTasks;
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
		friend class FSimpleCallbackTask;
		friend class FGroupRangeCallbackTask;
		friend class FGroupPrepareRangeTask;
		friend class FGroupRangeIterationTask;
		friend class FGroupPrepareRangeInlineTask;
		friend class FGroupRangeInlineIterationTask;

		FName GroupName = NAME_None;

	public:
		int8 bAvailable = 1;
		using SimpleCallback = std::function<void()>;

		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		using IterationCallback = std::function<void(const int32, const int32, const int32)>;
		IterationCallback OnIterationCallback;

		using PrepareSubLoopsCallback = std::function<void(const TArray<uint64>&)>;
		PrepareSubLoopsCallback OnPrepareSubLoopsCallback;

		using SubLoopStartCallback = std::function<void(const int32, const int32, const int32)>;
		SubLoopStartCallback OnSubLoopStartCallback;

		explicit FTaskGroup(const TSharedPtr<FTaskManager>& InManager, const FName InGroupName):
			GroupName(InGroupName), Manager(InManager)
		{
			InManager->GrowNumStarted();
		}

		~FTaskGroup()
		{
		}

		bool IsAvailable() const;

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			TArray<uint64> Loops;
			GrowNumStarted(SubRanges(Loops, MaxItems, ChunkSize));

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			TSharedPtr<FTaskGroup> SharedPtr = SharedThis(this);
			int32 LoopIdx = 0;
			for (const uint64 H : Loops)
			{
				FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, std::forward<Args>(InArgs)...);
				ATask->GetTask().GroupPtr = SharedPtr;
				ATask->GetTask().Scope = H;

				if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, LoopIdx++); }
				else { Manager->StartBackgroundTask<T>(ATask, LoopIdx++); }
			}
		}

		void StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bInlined = false, const bool bExecuteSmallSynchronously = true);

		void StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bInline = false);

		void AddSimpleCallback(SimpleCallback&& InCallback);
		void StartSimpleCallbacks();

		void GrowNumStarted(const int32 InNumStarted = 1);
		void GrowNumCompleted();

	protected:
		TArray<SimpleCallback> SimpleCallbacks;

		TSharedPtr<FTaskManager> Manager;

		mutable FRWLock GroupLock;
		mutable FRWLock TrackingLock;

		int32 NumStarted = 0;
		int32 NumCompleted = 0;

		void PrepareRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;
		void DoRangeIteration(const int32 StartIndex, const int32 Count, const int32 LoopIdx) const;

		template <typename T, typename... Args>
		void InternalStart(const bool bGrowNumStarted, const int32 TaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args&&... InArgs)
		{
			if (!Manager->IsAvailable()) { return; }

			if (bGrowNumStarted) { GrowNumStarted(); }
			FAsyncTask<T>* ATask = new FAsyncTask<T>(InPointsIO, std::forward<Args>(InArgs)...);
			ATask->GetTask().GroupPtr = SharedThis(this);
			if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(ATask, TaskIndex); }
			else { Manager->StartBackgroundTask<T>(ATask, TaskIndex); }
		}

		template <typename T>
		void InternalStartInlineRange(const int32 Index, const TArray<uint64>& Loops)
		{
			FAsyncTask<T>* NextRange = new FAsyncTask<T>(nullptr);
			NextRange->GetTask().GroupPtr = SharedThis(this);
			NextRange->GetTask().Loops = Loops;

			if (Manager->ForceSync) { Manager->StartSynchronousTask<T>(NextRange, Index); }
			else { Manager->StartBackgroundTask<T>(NextRange, Index); }
		}
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
		void InternalStart(int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointsIO, Args&&... InArgs)
		{
			if (const TSharedPtr<FTaskGroup> Group = GroupPtr.Pin())
			{
				Group->InternalStart<T>(true, InTaskIndex, InPointsIO, std::forward<Args>(InArgs)...);
			}
			else if (const TSharedPtr<FTaskManager> Manager = ManagerPtr.Pin())
			{
				Manager->Start<T>(InTaskIndex, InPointsIO, std::forward<Args>(InArgs)...);
			}
		}
	};

	class FSimpleCallbackTask final : public FPCGExTask
	{
	public:
		explicit FSimpleCallbackTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupRangeCallbackTask final : public FPCGExTask
	{
	public:
		explicit FGroupRangeCallbackTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		int32 NumIterations = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupRangeIterationTask final : public FPCGExTask
	{
	public:
		explicit FGroupRangeIterationTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupPrepareRangeTask final : public FPCGExTask
	{
	public:
		explicit FGroupPrepareRangeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		uint64 Scope = 0;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupPrepareRangeInlineTask final : public FPCGExTask
	{
	public:
		explicit FGroupPrepareRangeInlineTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		TArray<uint64> Loops;
		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FGroupRangeInlineIterationTask final : public FPCGExTask
	{
	public:
		explicit FGroupRangeInlineIterationTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
			FPCGExTask(InPointIO)
		{
		}

		TArray<uint64> Loops;
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
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTaskWithManager final : public FPCGExTask
	{
	public:
		FWriteTaskWithManager(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                      const TSharedPtr<T>& InOperation)
			: FPCGExTask(InPointIO),
			  Operation(InOperation)

		{
		}

		TSharedPtr<T> Operation;

		virtual bool ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override
		{
			if (!Operation) { return false; }
			Operation->Write(AsyncManager);
			return true;
		}
	};

	template <typename T, bool bWithManager = false>
	static void Write(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<T>& Operation)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable()) { Operation->Write(); }
		else
		{
			if constexpr (bWithManager) { AsyncManager->Start<FWriteTaskWithManager<T>>(-1, nullptr, Operation); }
			else { AsyncManager->Start<FWriteTask<T>>(-1, nullptr, Operation); }
		}
	}
}
