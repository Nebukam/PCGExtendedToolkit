// Copyright 2024 Timothé Lapetite and contributors
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

#ifndef PCGEX_MT_MACROS
#define PCGEX_MT_MACROS

#define PCGEX_ASYNC_CHECK(_MANAGER) if(!_MANAGER->IsAlive()){return false;}
#define PCGEX_ASYNC_CHECK_VOID(_MANAGER) if(!_MANAGER->IsAlive()){return;}

#define PCGEX_ASYNC_GROUP_CHKD_VOID(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return; }

#define PCGEX_ASYNC_GROUP_CHKD(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return false; }

#define PCGEX_ASYNC_THIS_DECL TWeakPtr<std::remove_reference_t<decltype(*this)>> AsyncThis = SharedThis(this);
#define PCGEX_ASYNC_THIS_CAPTURE AsyncThis = TWeakPtr<std::remove_reference_t<decltype(*this)>>(SharedThis(this))
#define PCGEX_ASYNC_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return; }
#define PCGEX_ASYNC_NESTED_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> NestedThis = AsyncThis.Pin(); if (!NestedThis) { return; }

#define PCGEX_START_TASK(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); AsyncManager->Start<_CLASS>(Task);
#define PCGEX_START_TASK_INTERNAL(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); InternalStart<_CLASS>(AsyncManager, InGroup, Task);

#endif
#pragma endregion

namespace PCGExMT
{
	static void SetWorkPriority(const EPCGExAsyncPriority Selection, UE::Tasks::ETaskPriority& Priority)
	{
		switch (Selection)
		{
		default:
		case EPCGExAsyncPriority::Default:
			SetWorkPriority(GetDefault<UPCGExGlobalSettings>()->GetDefaultWorkPriority(), Priority);
			break;
		case EPCGExAsyncPriority::Normal:
			Priority = UE::Tasks::ETaskPriority::Normal;
			break;
		case EPCGExAsyncPriority::High:
			Priority = UE::Tasks::ETaskPriority::High;
			break;
		case EPCGExAsyncPriority::BackgroundHigh:
			Priority = UE::Tasks::ETaskPriority::BackgroundHigh;
			break;
		case EPCGExAsyncPriority::BackgroundNormal:
			Priority = UE::Tasks::ETaskPriority::BackgroundNormal;
			break;
		case EPCGExAsyncPriority::BackgroundLow:
			Priority = UE::Tasks::ETaskPriority::BackgroundLow;
			break;
		case EPCGExAsyncPriority::Count:
			Priority = UE::Tasks::ETaskPriority::Count;
			break;
		}
	}

	struct FScope
	{
		int32 Start = -1;
		int32 Count = -1;
		int32 End = -1;
		int32 LoopIndex = -1;

		FScope() = default;

		FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex = -1)
			: Start(InStart), Count(InCount), End(InStart + InCount), LoopIndex(InLoopIndex)
		{
		}

		~FScope() = default;
		bool IsValid() const { return Start != -1 && Count > 0; }
		int32 GetNextScopeIndex() const { return LoopIndex + 1; }
	};

	static int32 SubLoopScopes(TArray<FScope>& OutSubRanges, const int32 MaxItems, const int32 RangeSize)
	{
		OutSubRanges.Empty();
		OutSubRanges.Reserve((MaxItems + RangeSize - 1) / RangeSize);

		for (int32 CurrentCount = 0; CurrentCount < MaxItems; CurrentCount += RangeSize)
		{
			OutSubRanges.Emplace(CurrentCount, FMath::Min(RangeSize, MaxItems - CurrentCount), OutSubRanges.Num());
		}

		return OutSubRanges.Num();
	}


	class FPCGExTask;
	class FTaskGroup;

	class /*PCGEXTENDEDTOOLKIT_API*/ FTaskManager : public TSharedFromThis<FTaskManager>
	{
		friend class FPCGExTask;
		friend class FTaskGroup;

		TSharedPtr<PCGEx::FLifecycle> Lifecycle;

	public:
		FTaskManager(FPCGExContext* InContext)
			: Context(InContext)
		{
			PCGEX_LOG_CTR(FTaskManager)
			Lifecycle = InContext->Lifecycle;
		}

		~FTaskManager();

		UE::Tasks::ETaskPriority WorkPriority = UE::Tasks::ETaskPriority::Default;

		mutable FRWLock ManagerLock;
		mutable FRWLock PauseLock;
		mutable FRWLock GroupLock;

		FPCGExContext* Context = nullptr;

		int8 Stopped = 0;
		int8 ForceSync = 0;

		void GrowNumStarted();
		void GrowNumCompleted();

		TSharedPtr<FTaskGroup> TryCreateGroup(const FName& GroupName);

		FORCEINLINE bool IsAlive() const { return Lifecycle->IsAlive(); }
		FORCEINLINE bool IsAvailable() const { return Stopped ? false : Lifecycle->IsAlive(); }

		template <typename T>
		void Start(const TSharedPtr<T>& InTask)
		{
			if (!IsAvailable()) { return; }
			if (ForceSync) { StartSynchronousTask(InTask); }
			else { StartBackgroundTask(InTask); }
		}

		template <typename T>
		void StartSynchronous(const TSharedPtr<T>& InTask)
		{
			if (!IsAvailable()) { return; }
			StartSynchronousTask(InTask);
		}

	protected:
		void StartBackgroundTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup = nullptr);
		void StartSynchronousTask(const TSharedPtr<FPCGExTask>& InTask, const TSharedPtr<FTaskGroup>& InGroup = nullptr);

	public:
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
		TArray<TSharedPtr<FPCGExTask>> QueuedTasks;
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
		friend class FScopeIterationTask;
		friend class FInlinedScopeIterationTask;

		FName GroupName = NAME_None;

	public:
		using SimpleCallback = std::function<void()>;

		using CompletionCallback = std::function<void()>;
		CompletionCallback OnCompleteCallback;

		using IterationCallback = std::function<void(const int32, const FScope&)>;
		IterationCallback OnIterationCallback;

		using PrepareSubLoopsCallback = std::function<void(const TArray<FScope>&)>;
		PrepareSubLoopsCallback OnPrepareSubLoopsCallback;

		using SubLoopStartCallback = std::function<void(const FScope&)>;
		SubLoopStartCallback OnSubLoopStartCallback;

		explicit FTaskGroup(const TSharedPtr<FTaskManager>& InManager, const FName InGroupName):
			GroupName(InGroupName), Manager(InManager)
		{
			InManager->GrowNumStarted();
		}

		~FTaskGroup()
		{
		}

		void Cancel()
		{
			bCanceled = true;
		}

		bool IsCanceled() const
		{
			return bCanceled;
		}

		bool IsAvailable() const;

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, const bool bPrepareOnly, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			TArray<FScope> Loops;
			GrowNumStarted(SubLoopScopes(Loops, MaxItems, ChunkSize));

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			const TSharedPtr<FTaskGroup> Self = SharedThis(this);

			for (const FScope& Scope : Loops)
			{
				PCGEX_MAKE_SHARED(Task, T, std::forward<Args>(InArgs)...)
				Task->bPrepareOnly = bPrepareOnly;
				Task->Scope = Scope;

				if (Manager->ForceSync) { Manager->StartSynchronousTask(Task, Self); }
				else { Manager->StartBackgroundTask(Task, Self); }
			}
		}

		void StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bInlined = false);
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

		void PrepareRangeIteration(const FScope& Scope) const;
		void DoRangeIteration(const FScope& Scope, bool bPrepareOnly) const;

		template <typename T>
		void InternalStart(TSharedPtr<T> InTask, const bool bGrowNumStarted)
		{
			if (!IsAvailable()) { return; } // Redundant?

			if (bGrowNumStarted) { GrowNumStarted(); }

			if (Manager->ForceSync) { Manager->StartSynchronousTask(InTask, SharedThis(this)); }
			else { Manager->StartBackgroundTask(InTask, SharedThis(this)); }
		}

		template <typename T>
		void InternalStartInlineRange(TSharedPtr<T> InTask, const bool bPrepareOnly, const TArray<FScope>& Loops)
		{
			if (!IsAvailable()) { return; } // Redundant?

			// Expects FPCGExIndexedTask

			InTask->bPrepareOnly = bPrepareOnly;
			InTask->Loops = Loops;

			if (Manager->ForceSync) { Manager->StartSynchronousTask(InTask, SharedThis(this)); }
			else { Manager->StartBackgroundTask(InTask, SharedThis(this)); }
		}

	private:
		std::atomic<bool> bCanceled{false};
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTask : public TSharedFromThis<FPCGExTask>
	{
		friend class FTaskManager;
		friend class FTaskGroup;

	public:
		FPCGExTask() = default;
		virtual ~FPCGExTask() = default;

		FString GetTaskName() const;

		void Cancel()
		{
			bCanceled = true;
		}

		bool IsCanceled() const
		{
			return bCanceled;
		}

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) = 0;

	protected:
		template <typename T>
		static void InternalStart(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup, const TSharedPtr<T>& InTask)
		{
			if (InGroup) { InGroup->InternalStart<T>(InTask, true); }
			else if (AsyncManager) { AsyncManager->Start<T>(InTask); }
		}

	private:
		std::atomic<bool> bCanceled{false};
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIndexedTask : public FPCGExTask
	{
	protected:
		int32 TaskIndex;

	public:
		explicit FPCGExIndexedTask(const int32 InTaskIndex)
			: FPCGExTask(), TaskIndex(InTaskIndex)
		{
		}
	};

	class FSimpleCallbackTask final : public FPCGExIndexedTask
	{
	public:
		explicit FSimpleCallbackTask(const int32 InTaskIndex):
			FPCGExIndexedTask(InTaskIndex)
		{
		}

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) override
		{
			if (InGroup) { InGroup->SimpleCallbacks[TaskIndex](); }
		}
	};

	class FScopeIterationTask : public FPCGExTask
	{
	public:
		explicit FScopeIterationTask() : FPCGExTask()
		{
		}

		bool bPrepareOnly = false;
		FScope Scope = FScope{};

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) override
		{
			if (!InGroup) { return; }
			InGroup->DoRangeIteration(Scope, bPrepareOnly);
		}
	};

	class FInlinedScopeIterationTask final : public FPCGExIndexedTask
	{
	public:
		explicit FInlinedScopeIterationTask(const int32 InTaskIndex):
			FPCGExIndexedTask(InTaskIndex)
		{
		}

		bool bPrepareOnly = false;
		TArray<FScope> Loops;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTask final : public FPCGExTask
	{
	public:
		explicit FWriteTask(const TSharedPtr<T>& InOperation)
			: FPCGExTask(), Operation(InOperation)
		{
		}

		TSharedPtr<T> Operation;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) override
		{
			if (!Operation) { return; }
			Operation->Write();
		}
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTaskWithManager final : public FPCGExTask
	{
	public:
		FWriteTaskWithManager(const TSharedPtr<T>& InOperation)
			: FPCGExTask(), Operation(InOperation)
		{
		}

		TSharedPtr<T> Operation;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<FTaskGroup>& InGroup) override
		{
			if (!Operation) { return; }
			Operation->Write(AsyncManager);
		}
	};

	template <typename T, bool bWithManager = false>
	static void Write(const TSharedPtr<FTaskManager>& AsyncManager, const TSharedPtr<T>& Operation)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable()) { Operation->Write(); }
		else
		{
			if constexpr (bWithManager) { PCGEX_START_TASK(FWriteTaskWithManager<T>, Operation) }
			else { PCGEX_START_TASK(FWriteTask<T>, Operation) }
		}
	}
}
