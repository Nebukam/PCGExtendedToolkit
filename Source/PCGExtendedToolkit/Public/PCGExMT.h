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

#define PCGEX_ASYNC_TASK_NAME(_NAME) virtual FString HandleId() const override { return TEXT(""#_NAME); }

#define PCGEX_ASYNC_GROUP_CHKD_VOID(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return; }

#define PCGEX_ASYNC_GROUP_CHKD(_MANAGER, _NAME) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return false; }

#define PCGEX_ASYNC_GROUP_CHKD_CUSTOM(_MANAGER, _NAME, _RET) \
	TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; \
	if(!_NAME){ return _RET; }

#define PCGEX_ASYNC_RELEASE_TOKEN(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }
#define PCGEX_ASYNC_RELEASE_TOKEN_ELSE(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }else

#define PCGEX_SHARED_THIS_DECL TSharedPtr<std::remove_reference_t<decltype(*this)>> ThisPtr = SharedThis(this);
#define PCGEX_ASYNC_THIS_DECL TWeakPtr<std::remove_reference_t<decltype(*this)>> AsyncThis = SharedThis(this);
#define PCGEX_ASYNC_THIS_CAPTURE AsyncThis = TWeakPtr<std::remove_reference_t<decltype(*this)>>(SharedThis(this))
#define PCGEX_ASYNC_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return; }
#define PCGEX_ASYNC_NESTED_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> NestedThis = AsyncThis.Pin(); if (!NestedThis) { return; }

#define PCGEX_ASYNC_CHKD_VOID(_MANAGER) if (!_MANAGER->IsAvailable()) { return; }
#define PCGEX_ASYNC_CHKD(_MANAGER) if (!_MANAGER->IsAvailable()) { return false; }

#define PCGEX_LAUNCH(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); AsyncManager->Launch<_CLASS>(Task);
#define PCGEX_LAUNCH_INTERNAL(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); Launch<_CLASS>(Task);

#endif
#pragma endregion

namespace PCGExMT
{
	using FCompletionCallback = std::function<void()>;

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

	enum class EAsyncHandleState : uint8
	{
		Idle    = 0,
		Running = 1,
		Ended   = 2,
	};

	class FAsyncMultiHandle;
	class FAsyncToken;
	class FTask;
	class FTaskGroup;

	class /*PCGEXTENDEDTOOLKIT_API*/ FAsyncHandle : public TSharedFromThis<FAsyncHandle>
	{
	protected:
		TWeakPtr<FAsyncMultiHandle> Root;
		TWeakPtr<FAsyncMultiHandle> ParentHandle;

		std::atomic<bool> bIsCancelled{false};
		std::atomic<EAsyncHandleState> State{EAsyncHandleState::Idle};

	public:
		virtual FString HandleId() const { return TEXT("NOT IMPLEMENTED"); }

		FAsyncHandle() = default;
		virtual ~FAsyncHandle();

		bool IsCancelled() const { return bIsCancelled.load(std::memory_order_acquire); }

		virtual void SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot);
		void SetParent(const TSharedPtr<FAsyncMultiHandle>& InParent);

		virtual bool Start();    // Return whether the task is running or not
		virtual bool Cancel();   // Return whether the task is ended or not
		virtual bool Complete(); // Return whether the task is ended or not

		void SetState(const EAsyncHandleState NewState) { State.store(NewState, std::memory_order_release); }
		EAsyncHandleState GetState() const { return State.load(std::memory_order_acquire); }

	protected:
		bool CompareAndSetState(EAsyncHandleState& ExpectedState, EAsyncHandleState NewState);
		virtual void End(bool bIsCancellation);
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FAsyncMultiHandle : public FAsyncHandle
	{
	protected:
		bool bForceSync = false;
		FName GroupName = NAME_None;

	public:
		explicit FAsyncMultiHandle(const bool InForceSync, const FName InName);
		virtual ~FAsyncMultiHandle() override;

		FCompletionCallback OnCompleteCallback; // Only called with handle was not cancelled

		virtual void SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot) override;

		void IncrementPendingTasks();
		void IncrementCompletedTasks();

		virtual bool IsAvailable() const;

		template <typename T>
		void Launch(const TSharedPtr<T>& InTask)
		{
			if (bForceSync) { StartSynchronousTask(InTask); }
			else { StartBackgroundTask(InTask); }
		}

	protected:
		std::atomic<int32> ExpectedTaskCount = {0};
		std::atomic<int32> PendingTaskCount{0};
		std::atomic<int32> CompletedTaskCount{0};

		void SetExpectedTaskCount(const int32 InCount) { ExpectedTaskCount.store(InCount, std::memory_order_release); }

		virtual void HandleTaskStart();

		virtual void StartBackgroundTask(const TSharedPtr<FTask>& InTask);
		virtual void StartSynchronousTask(const TSharedPtr<FTask>& InTask);

		virtual void End(bool bIsCancellation) override;
		virtual void Reset();
	};

	class FAsyncToken final : public TSharedFromThis<FAsyncToken>
	{
		std::atomic<bool> bIsReleased{false};
		TWeakPtr<FAsyncMultiHandle> Handle;
		FName Name = NAME_None;

	public:
		FAsyncToken(const TWeakPtr<FAsyncMultiHandle>& InHandle, const FName& InName);
		~FAsyncToken();

		void Release();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTaskManager : public FAsyncMultiHandle
	{
		friend class FTask;
		friend class FTaskGroup;

		mutable FRWLock TasksLock;
		mutable FRWLock GroupsLock;
		mutable FRWLock TokensLock;

		TWeakPtr<PCGEx::FWorkPermit> WorkPermit;

		FPCGExContext* Context = nullptr;

	public:
		UE::Tasks::ETaskPriority WorkPriority = UE::Tasks::ETaskPriority::Default;

		explicit FTaskManager(FPCGExContext* InContext, bool InForceSync = false);
		virtual ~FTaskManager() override;

		virtual bool IsAvailable() const override;

		bool IsWaitingForRunningTasks() const;

		template <typename T>
		T* GetContext() const { return static_cast<T*>(Context); }

		FPCGExContext* GetContext() const { return Context; }

		virtual bool Start() override;
		virtual bool Cancel() override;

		virtual void Reset() override;
		void FlushTasks();

		void ReserveTasks(const int32 NumTasks);

		TSharedPtr<FTaskGroup> TryCreateTaskGroup(const FName& InName);
		TWeakPtr<FAsyncToken> TryCreateToken(const FName& TokenName);

	protected:
		std::atomic<bool> bIsCancelling{false};
		bool IsCancelling() const { return bIsCancelling.load(std::memory_order_acquire); }

		std::atomic<bool> bIsResetting{false};
		bool IsResetting() const { return bIsResetting.load(std::memory_order_acquire); }

		TArray<TSharedPtr<FTask>> Tasks;
		TArray<TSharedPtr<FTaskGroup>> Groups;
		TArray<TSharedPtr<FAsyncToken>> Tokens;

		virtual void HandleTaskStart() override;
		virtual void End(bool bIsCancellation) override;

		virtual void StartBackgroundTask(const TSharedPtr<FTask>& InTask) override;
		virtual void StartSynchronousTask(const TSharedPtr<FTask>& InTask) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTaskGroup final : public FAsyncMultiHandle
	{
		friend class FTaskManager;

		friend class FTask;
		friend class FSimpleCallbackTask;
		friend class FScopeIterationTask;
		friend class FDaisyChainScopeIterationTask;

	public:
		using FSimpleCallback = std::function<void()>;

		using FIterationCallback = std::function<void(const int32, const FScope&)>;
		FIterationCallback OnIterationCallback;

		using FPrepareSubLoopsCallback = std::function<void(const TArray<FScope>&)>;
		FPrepareSubLoopsCallback OnPrepareSubLoopsCallback;

		using FSubLoopStartCallback = std::function<void(const FScope&)>;
		FSubLoopStartCallback OnSubLoopStartCallback;

		explicit FTaskGroup(const bool InForceSync, const FName InName);

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, const bool bPrepareOnly, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin();
			if (!PinnedRoot) { return; }

			check(MaxItems > 0);

			// Compute sub scopes
			SetExpectedTaskCount(SubLoopScopes(Loops, MaxItems, FMath::Max(1, ChunkSize)));
			StaticCastSharedPtr<FTaskManager>(PinnedRoot)->ReserveTasks(Loops.Num());

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			const TSharedPtr<FTaskGroup> Self = SharedThis(this);

			for (const FScope& Scope : Loops)
			{
				PCGEX_MAKE_SHARED(Task, T, std::forward<Args>(InArgs)...)
				Task->bPrepareOnly = bPrepareOnly;
				Task->Scope = Scope;

				Launch(Task);
			}

			Loops.Empty();
		}

		void StartIterations(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain = false);
		void StartSubLoops(const int32 MaxItems, const int32 ChunkSize, const bool bDaisyChain = false);

		void AddSimpleCallback(FSimpleCallback&& InCallback);
		void StartSimpleCallbacks();
		void TriggerSimpleCallback(const int32 Index);

		const TArray<FScope>& GetLoopScopes() const { return Loops; }
		const FScope& GetLoopScope(int32 Index) const { return Loops[Index]; }

	protected:
		bool bDaisyChained = false;
		TArray<FSimpleCallback> SimpleCallbacks;
		TArray<FScope> Loops;

		void ExecScopeIterations(const FScope& Scope, bool bPrepareOnly) const;

		template <typename T>
		void LaunchWithPreparation(TSharedPtr<T> InTask, const bool bPrepareOnly)
		{
			InTask->bPrepareOnly = bPrepareOnly;
			Launch(InTask);
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FTask : public FAsyncHandle
	{
		friend class FTaskManager;
		friend class FTaskGroup;

		mutable FRWLock StateLock;

	public:
		FTask() = default;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) = 0;

	protected:
		template <typename T>
		void Launch(const TSharedPtr<T>& InTask)
		{
			if (const TSharedPtr<FAsyncMultiHandle> PinnedParent = ParentHandle.Pin())
			{
				PinnedParent->Launch<T>(InTask);
			}
			else if (const TSharedPtr<FAsyncMultiHandle> PinnedRoot = Root.Pin())
			{
				PinnedRoot->Launch<T>(InTask);
			}
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIndexedTask : public FTask
	{
	protected:
		int32 TaskIndex;

	public:
		PCGEX_ASYNC_TASK_NAME(FPCGExIndexedTask)

		explicit FPCGExIndexedTask(const int32 InTaskIndex)
			: FTask(), TaskIndex(InTaskIndex)
		{
		}
	};

	class FSimpleCallbackTask final : public FPCGExIndexedTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FSimpleCallbackTask)

		explicit FSimpleCallbackTask(const int32 InTaskIndex):
			FPCGExIndexedTask(InTaskIndex)
		{
		}

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FScopeIterationTask : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FScopeIterationTask)

		explicit FScopeIterationTask() : FTask()
		{
		}

		bool bPrepareOnly = false;
		FScope Scope = FScope{};

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class FDaisyChainScopeIterationTask final : public FPCGExIndexedTask
	{
	public:
		explicit FDaisyChainScopeIterationTask(const int32 InTaskIndex):
			FPCGExIndexedTask(InTaskIndex)
		{
		}

		bool bPrepareOnly = false;
		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTask final : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteTask)

		explicit FWriteTask(const TSharedPtr<T>& InOperation)
			: FTask(), Operation(InOperation)
		{
		}


		TSharedPtr<T> Operation;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override
		{
			if (!Operation) { return; }
			Operation->Write();
		}
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteTaskWithManager final : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteTaskWithManager)

		FWriteTaskWithManager(const TSharedPtr<T>& InOperation)
			: FTask(), Operation(InOperation)
		{
		}

		TSharedPtr<T> Operation;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override
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
			if constexpr (bWithManager) { PCGEX_LAUNCH(FWriteTaskWithManager<T>, Operation) }
			else { PCGEX_LAUNCH(FWriteTask<T>, Operation) }
		}
	}
}
