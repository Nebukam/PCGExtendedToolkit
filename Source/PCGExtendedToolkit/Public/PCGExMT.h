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
#include "Tasks/Task.h"


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
	using FSimpleCallback = std::function<void()>;

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


	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedArray final : public TSharedFromThis<TScopedArray<T>>
	{
	public:
		TArray<TSharedPtr<TArray<T>>> Values;

		explicit TScopedArray(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Values.Reserve(InScopes.Num());
			for (const FScope& Scope : InScopes) { Values[Values.Add(MakeShared<TArray<T>>())]->Init(InDefaultValue, Scope.Count); }
		};

		explicit TScopedArray(const TArray<FScope>& InScopes)
		{
			Values.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Values.Add(MakeShared<TArray<T>>()); }
		};

		virtual ~TScopedArray() = default;

		FORCEINLINE TSharedPtr<TArray<T>> Get(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE TArray<T>& Get_Ref(const FScope& InScope) { return *Values[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (TArray<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Values.Num(); i++) { Func(*Values[i].Get()); } }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedSet final : public TSharedFromThis<TScopedSet<T>>
	{
	public:
		TArray<TSharedPtr<TSet<T>>> Sets;

		explicit TScopedSet(const TArray<FScope>& InScopes, const T InReserve)
		{
			Sets.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Sets.Add_GetRef(MakeShared<TSet<T>>())->Reserve(InReserve); }
		};

		virtual ~TScopedSet() = default;

		FORCEINLINE TSharedPtr<TSet<T>> Get(const FScope& InScope) { return Sets[InScope.LoopIndex]; }
		FORCEINLINE TSet<T>& Get_Ref(const FScope& InScope) { return *Sets[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (TSet<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Sets.Num(); i++) { Func(*Sets[i].Get()); } }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedValue final : public TSharedFromThis<TScopedValue<T>>
	{
	public:
		TArray<T> Values;

		using FFlattenFunc = std::function<T(const T&, const T&)>;

		explicit TScopedValue(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Values.Init(InDefaultValue, InScopes.Num());
		};

		virtual ~TScopedValue() = default;

		FORCEINLINE T Get(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T& GetMutable(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T Set(const FScope& InScope, const T& InValue) { return Values[InScope.LoopIndex] = InValue; }

		FORCEINLINE T Flatten(FFlattenFunc&& Func)
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result = Func(Values[i], Result); } }
			return Result;
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FAsyncHandle : public TSharedFromThis<FAsyncHandle>
	{
	protected:
		TWeakPtr<FAsyncMultiHandle> Root;
		TWeakPtr<FAsyncMultiHandle> ParentHandle;

		std::atomic<bool> bIsCancelled{false};
		std::atomic<EAsyncHandleState> State{EAsyncHandleState::Idle};

	public:
		int32 HandleIdx = -1;
		virtual FString HandleId() const { return TEXT("NOT IMPLEMENTED"); }

		FAsyncHandle() = default;
		virtual ~FAsyncHandle();

		bool IsCancelled() const { return bIsCancelled.load(std::memory_order_acquire); }

		virtual void SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, int32 InHandleIdx = -1);
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

		virtual void SetRoot(const TSharedPtr<FAsyncMultiHandle>& InRoot, int32 InHandleIdx) override;

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

		void ReserveTasks(const int32 NumTasks);

		TSharedPtr<FTaskGroup> TryCreateTaskGroup(const FName& InName);
		TWeakPtr<FAsyncToken> TryCreateToken(const FName& TokenName);

	protected:
		std::atomic<bool> bIsCancelling{false};
		bool IsCancelling() const { return bIsCancelling.load(std::memory_order_acquire); }

		std::atomic<bool> bIsResetting{false};
		bool IsResetting() const { return bIsResetting.load(std::memory_order_acquire); }

		TArray<TWeakPtr<FTask>> Tasks;
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

	class /*PCGEXTENDEDTOOLKIT_API*/ FDeferredCallbackTask final : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FDeferredCallbackTask)

		FDeferredCallbackTask(FSimpleCallback&& InCallback)
			: FTask(), Callback(InCallback)
		{
		}

		FSimpleCallback Callback;
		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FDeferredCallbackWithManagerTask final : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FDeferredCallbackWithManagerTask)

		using FSimpleCallbackWithManager = std::function<void(const TSharedPtr<FTaskManager>&)>;

		FDeferredCallbackWithManagerTask(FSimpleCallbackWithManager&& InCallback)
			: FTask(), Callback(InCallback)
		{
		}

		FSimpleCallbackWithManager Callback;
		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& AsyncManager) override;
	};

	//

	class FDeferredCallbackHandle : public FAsyncHandle
	{
	public:
		FSimpleCallback Callback;

		explicit FDeferredCallbackHandle() : FAsyncHandle()
		{
		}

		virtual bool Start() override;
	};

	static TSharedPtr<FDeferredCallbackHandle> DeferredCallback(FPCGExContext* InContext, FSimpleCallback&& InCallback)
	{
		TSharedPtr<FDeferredCallbackHandle> DeferredCallback = MakeShared<FDeferredCallbackHandle>();
		DeferredCallback->Callback = InCallback;

		TWeakPtr<FDeferredCallbackHandle> WeakTask = DeferredCallback;
		UE::Tasks::Launch(
				TEXT("DeferredCallback"),
				[WeakTask]()
				{
					const TSharedPtr<FDeferredCallbackHandle> Task = WeakTask.Pin();
					if (!Task.IsValid()) { return; }
					if (Task->Start()) { Task->Complete(); }
				},
				UE::Tasks::ETaskPriority::Default
			);

		return DeferredCallback;
	}

	static void CancelDeferredCallback(const TSharedPtr<FDeferredCallbackHandle>& InCallback)
	{
		InCallback->Cancel();
		while (InCallback->GetState() != EAsyncHandleState::Ended)
		{
			// Hold off until ended
		}
	}

}
