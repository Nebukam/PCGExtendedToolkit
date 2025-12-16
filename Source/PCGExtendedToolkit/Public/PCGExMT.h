// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>
#include <atomic>

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"
#include "Async/AsyncWork.h"
#include "Misc/QueuedThreadPool.h"
#include "Tasks/Task.h"

#include "Details/PCGExMacros.h"

#pragma region MT MACROS

#ifndef PCGEX_MT_MACROS
#define PCGEX_MT_MACROS

#define PCGEX_ASYNC_TASK_NAME(_NAME) virtual FString DEBUG_HandleId() const override { return TEXT(""#_NAME); }

#define PCGEX_ASYNC_GROUP_CHKD_VOID(_MANAGER, _NAME) TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; if(!_NAME){ return; }
#define PCGEX_ASYNC_SUBGROUP_CHKD_RET(_MANAGER, _PARENT, _NAME, _RET) TSharedPtr<PCGExMT::FTaskGroup> _NAME = _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME), _PARENT) : nullptr; if(!_NAME){ return _RET; }
#define PCGEX_ASYNC_SUBGROUP_REQ_CHKD_VOID(_MANAGER, _PARENT, _NAME) TSharedPtr<PCGExMT::FTaskGroup> _NAME = (_MANAGER && _PARENT) ? _MANAGER->TryCreateTaskGroup(FName(#_NAME), _PARENT) : nullptr; if(!_NAME){ return; }
#define PCGEX_ASYNC_GROUP_CHKD_RET(_MANAGER, _NAME, _RET) TSharedPtr<PCGExMT::FTaskGroup> _NAME= _MANAGER ? _MANAGER->TryCreateTaskGroup(FName(#_NAME)) : nullptr; if(!_NAME){ return _RET; }
#define PCGEX_ASYNC_GROUP_CHKD(_MANAGER, _NAME) PCGEX_ASYNC_GROUP_CHKD_RET(_MANAGER, _NAME, false);

#define PCGEX_ASYNC_HANDLE_CHKD_VOID(_MANAGER, _HANDLE) if (!_MANAGER->TryRegisterHandle(_HANDLE)){return;}
#define PCGEX_ASYNC_HANDLE_CHKD_CUSTOM(_MANAGER, _HANDLE, _RET) if (!_MANAGER->TryRegisterHandle(_HANDLE)){return _RET;}
#define PCGEX_ASYNC_HANDLE_CHKD(_MANAGER, _HANDLE) PCGEX_ASYNC_HANDLE_CHKD_CUSTOM(_MANAGER, _HANDLE, false);

#define PCGEX_ASYNC_RELEASE_TOKEN(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }
#define PCGEX_ASYNC_RELEASE_CAPTURED_TOKEN(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); }
#define PCGEX_ASYNC_RELEASE_TOKEN_ELSE(_TOKEN) if(const TSharedPtr<PCGExMT::FAsyncToken> Token = _TOKEN.Pin()){ Token->Release(); _TOKEN.Reset(); }else

#define PCGEX_SHARED_THIS_DECL TSharedPtr<std::remove_reference_t<decltype(*this)>> ThisPtr = SharedThis(this);
#define PCGEX_ASYNC_THIS_DECL TWeakPtr<std::remove_reference_t<decltype(*this)>> AsyncThis = SharedThis(this);
#define PCGEX_ASYNC_THIS_CAPTURE AsyncThis = TWeakPtr<std::remove_reference_t<decltype(*this)>>(SharedThis(this))
#define PCGEX_ASYNC_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return; }
#define PCGEX_ASYNC_THIS_RET(_RET) const TSharedPtr<std::remove_reference_t<decltype(*this)>> This = AsyncThis.Pin(); if (!This) { return _RET; }
#define PCGEX_ASYNC_NESTED_THIS const TSharedPtr<std::remove_reference_t<decltype(*this)>> NestedThis = AsyncThis.Pin(); if (!NestedThis) { return; }

#define PCGEX_ASYNC_CHKD_VOID(_MANAGER) if (!_MANAGER->IsAvailable()) { return; }
#define PCGEX_ASYNC_CHKD(_MANAGER) if (!_MANAGER->IsAvailable()) { return false; }

#define PCGEX_LAUNCH(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); TaskManager->Launch(Task);
#define PCGEX_LAUNCH_INTERNAL(_CLASS, ...) PCGEX_MAKE_SHARED(Task, _CLASS, __VA_ARGS__); Launch(Task);

#define PCGEX_SCOPE_LOOP(_VAR) for(int _VAR = Scope.Start; _VAR < Scope.End; _VAR++)
#define PCGEX_SUBSCOPE_LOOP(_VAR) for(int _VAR = SubScope.Start; _VAR < SubScope.End; _VAR++)

#endif
#pragma endregion

namespace PCGEx
{
	class FWorkHandle;
}

struct FPCGContextHandle;
enum class EPCGExAsyncPriority : uint8;
struct FPCGExContext;

namespace PCGExMT
{
	int32 SubLoopScopes(TArray<FScope>& OutSubRanges, const int32 NumIterations, const int32 RangeSize);

	enum class EAsyncHandleState : uint8
	{
		Idle    = 0,
		Running = 1,
		Ended   = 2,
	};

	class IAsyncHandleGroup;
	class FAsyncToken;
	class FTask;
	class FTaskGroup;
	class FTaskManager;

	// Base async handle with state management
	class PCGEXTENDEDTOOLKIT_API IAsyncHandle : public TSharedFromThis<IAsyncHandle>
	{
		friend class IAsyncHandleGroup;
		friend class FAsyncToken;
		friend class FTask;
		friend class FTaskGroup;
		friend class FTaskManager;

	protected:
		int8 bExpected = false;
		TWeakPtr<IAsyncHandleGroup> Group;

		std::atomic<bool> bResetting{false};
		std::atomic<bool> bCancelled{false};
		std::atomic<EAsyncHandleState> State{EAsyncHandleState::Idle};

	public:
		int32 HandleIdx = -1;
		virtual FString DEBUG_HandleId() const { return TEXT("NOT IMPLEMENTED"); }

		IAsyncHandle() = default;
		virtual ~IAsyncHandle();

		bool IsCancelled() const { return bCancelled.load(std::memory_order_acquire); }
		EAsyncHandleState GetState() const { return State.load(std::memory_order_acquire); }

		virtual FTaskManager* GetManager() const;
		bool SetGroup(const TSharedPtr<IAsyncHandleGroup>& InGroup);

		virtual bool Start();
		virtual void Cancel();
		virtual void Complete();

	protected:
		bool TryTransitionState(EAsyncHandleState From, EAsyncHandleState To);
		virtual void OnEnd(bool bWasCancelled);
	};

#define PCGEX_SCHEDULING_SCOPE(_MANAGER, ...) PCGExMT::FSchedulingScope SchedulingScope(_MANAGER); if(!SchedulingScope.Token.IsValid()) { return __VA_ARGS__; }

	struct FSchedulingScope
	{
		TWeakPtr<FAsyncToken> Token;
		explicit FSchedulingScope(const TSharedPtr<FTaskManager>& InManager);
		~FSchedulingScope();
	};

	// Multi-handle manages multiple child tasks
	class PCGEXTENDEDTOOLKIT_API IAsyncHandleGroup : public IAsyncHandle
	{
		friend class FTaskManager;

	protected:
		FName GroupName = NAME_None;

		// Per-handle registry for memory management

		mutable FRWLock RegistryLock;
		TArray<TWeakPtr<IAsyncHandle>> Registry;

		mutable FRWLock TokenLock;
		TArray<TSharedPtr<FAsyncToken>> Tokens;

		std::atomic<int32> PendingRegistrations{0};
		std::atomic<int32> ExpectedCount{0};
		std::atomic<int32> StartedCount{0};
		std::atomic<int32> CompletedCount{0};

	public:
		using FCreateLaunchablePredicate = std::function<TSharedPtr<FTask>(int32)>;

		// RAII guard that blocks CheckCompletion during registration
		struct FRegistrationGuard
		{
			TSharedPtr<IAsyncHandleGroup> Parent;

			explicit FRegistrationGuard(const TSharedPtr<IAsyncHandleGroup>& InParent)
				: Parent(InParent)
			{
				Parent->PendingRegistrations.fetch_add(1, std::memory_order_acquire);
			}

			~FRegistrationGuard()
			{
				const int32 Remaining = Parent->PendingRegistrations.fetch_sub(1, std::memory_order_release) - 1;
				if (Remaining == 0) { Parent->CheckCompletion(); }
			}
		};

		virtual FString DEBUG_HandleId() const override { return GroupName.ToString(); }
		FCompletionCallback OnCompleteCallback;

		explicit IAsyncHandleGroup(const FName InName);
		virtual ~IAsyncHandleGroup() override;

		virtual bool IsAvailable() const;

		bool RegisterExpected(int32 Count = 1);
		void NotifyStarted();
		void NotifyCompleted();

		void Launch(const TSharedPtr<FTask>& InTask, const bool bIsExpected = false);
		int32 Launch(const int32 Count, FCreateLaunchablePredicate&& Predicate);

		TWeakPtr<FAsyncToken> TryCreateToken(const FName& InName);

		virtual void Cancel() override;

	protected:
		virtual bool CanScheduleWork();
		virtual void LaunchInternal(const TSharedPtr<FTask>& InTask);
		virtual void OnEnd(bool bWasCancelled) override;
		void CheckCompletion();

		int32 RegisterTask(const TSharedPtr<IAsyncHandle>& InTask);
		virtual void ClearRegistry(const bool bCancel = false);

		void StartHandlesBatchImpl(const TArray<TSharedPtr<FTask>>& InHandles);

		void AssertEmptyThread() const;
	};

	// Token for async work tracking
	class PCGEXTENDEDTOOLKIT_API FAsyncToken final : public TSharedFromThis<FAsyncToken>
	{
		std::atomic<bool> bReleased{false};
		TWeakPtr<IAsyncHandleGroup> Group;

	public:
		FAsyncToken(const TWeakPtr<IAsyncHandleGroup>& InHandle);
		~FAsyncToken();
		void Release();
	};

	// Task manager - root of task hierarchy
	class PCGEXTENDEDTOOLKIT_API FTaskManager : public IAsyncHandleGroup
	{
		friend class IAsyncHandle;
		friend class IAsyncHandleGroup;
		friend class FTask;
		friend class FTaskGroup;

	protected:
		TWeakPtr<PCGEx::FWorkHandle> WorkHandle;
		FPCGExContext* Context = nullptr;
		TWeakPtr<FPCGContextHandle> ContextHandle;

		// Groups and tokens managed separately (they start immediately)
		mutable FRWLock GroupsLock;
		TArray<TSharedPtr<FTaskGroup>> Groups;

	public:
		FEndCallback OnEndCallback;
		UE::Tasks::ETaskPriority WorkPriority = UE::Tasks::ETaskPriority::Default;

		explicit FTaskManager(FPCGExContext* InContext);
		virtual ~FTaskManager() override;

		virtual FTaskManager* GetManager() const override;
		virtual bool IsAvailable() const override;
		bool IsWaitingForTasks() const;

		template <typename T>
		T* GetContext() const { return static_cast<T*>(Context); }

		FPCGExContext* GetContext() const { return Context; }

		virtual bool Start() override;
		virtual void Cancel() override;

		TSharedPtr<FTaskGroup> TryCreateTaskGroup(const FName& InName, const TSharedPtr<IAsyncHandleGroup>& InParentHandle = nullptr);
		bool TryRegisterHandle(const TSharedPtr<IAsyncHandle>& InHandle, const TSharedPtr<IAsyncHandleGroup>& InParentHandle = nullptr);

		void Reset();

	protected:
		virtual bool CanScheduleWork() override;
		virtual void LaunchInternal(const TSharedPtr<FTask>& InTask) override;
		virtual void OnEnd(bool bWasCancelled) override;

		virtual void ClearRegistry(const bool bCancel = false) override;

		void ClearGroups();
	};

	// Task group for batched operations
	class PCGEXTENDEDTOOLKIT_API FTaskGroup final : public IAsyncHandleGroup
	{
		friend class FTaskManager;
		friend class FSimpleCallbackTask;
		friend class FScopeIterationTask;
		friend class FForceSingleThreadedScopeIterationTask;

	public:
		using FIterationCallback = std::function<void(const int32, const FScope&)>;
		FIterationCallback OnIterationCallback;

		using FPrepareSubLoopsCallback = std::function<void(const TArray<FScope>&)>;
		FPrepareSubLoopsCallback OnPrepareSubLoopsCallback;

		using FSubLoopStartCallback = std::function<void(const FScope&)>;
		FSubLoopStartCallback OnSubLoopStartCallback;

		explicit FTaskGroup(const FName InName);

		template <typename T, typename... Args>
		void StartRanges(const int32 MaxItems, const int32 ChunkSize, const bool bPrepareOnly, Args&&... InArgs)
		{
			if (!IsAvailable()) { return; }

			if (!MaxItems)
			{
				AssertEmptyThread();
				return;
			}

			TArray<FScope> Loops;
			const int32 NumLoops = SubLoopScopes(Loops, MaxItems, FMath::Max(1, ChunkSize));

			if (OnPrepareSubLoopsCallback) { OnPrepareSubLoopsCallback(Loops); }

			Launch(NumLoops, [&](int32 i)
			{
				PCGEX_MAKE_SHARED(Task, T, std::forward<Args>(InArgs)...)
				Task->bPrepareOnly = bPrepareOnly;
				Task->Scope = Loops[i];
				return Task;
			});
		}

		void StartIterations(const int32 NumIterations, const int32 ChunkSize, const bool bForceSingleThreaded = false, const bool bPreparationOnly = false);
		void StartSubLoops(const int32 NumIterations, const int32 ChunkSize, const bool bForceSingleThreaded = false);

		void AddSimpleCallback(FSimpleCallback&& InCallback);
		void StartSimpleCallbacks();

		template <typename T>
		void StartTasksBatch(const TArray<TSharedPtr<T>>& InTasks)
		{
			static_assert(TIsDerivedFrom<T, FTask>::Value, "T must derive from IAsyncHandle");
			const TArray<TSharedPtr<FTask>>& BaseTasks = reinterpret_cast<const TArray<TSharedPtr<FTask>>&>(InTasks);
			StartHandlesBatchImpl(BaseTasks);
		}

	protected:
		TArray<FSimpleCallback> SimpleCallbacks;

		void ExecScopeIteration(const FScope& Scope, bool bPrepareOnly) const;
		void TriggerSimpleCallback(int32 Index);
	};

	PCGEXTENDEDTOOLKIT_API
	void ExecuteOnMainThread(const TSharedPtr<IAsyncHandleGroup>& ParentHandle, FExecuteCallback&& Callback);

	PCGEXTENDEDTOOLKIT_API
	void ExecuteOnMainThread(FExecuteCallback&& Callback);

	PCGEXTENDEDTOOLKIT_API
	void ExecuteOnMainThreadAndWait(FExecuteCallback&& Callback);

	// Base task class
	class PCGEXTENDEDTOOLKIT_API FTask : public IAsyncHandle
	{
		friend class FTaskManager;
		friend class FTaskGroup;

	public:
		FTask() = default;
		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& TaskManager) = 0;

	protected:
		void Launch(const TSharedPtr<FTask>& InTask, const bool bIsExpected = false) const;
	};

	// Indexed task base
	class PCGEXTENDEDTOOLKIT_API FPCGExIndexedTask : public FTask
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

	// Built-in task types
	class FSimpleCallbackTask final : public FPCGExIndexedTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FSimpleCallbackTask)

		explicit FSimpleCallbackTask(const int32 InTaskIndex)
			: FPCGExIndexedTask(InTaskIndex)
		{
		}

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& TaskManager) override;
	};

	class FScopeIterationTask : public FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FScopeIterationTask)
		bool bPrepareOnly = false;
		FScope Scope = FScope{};
		int32 NumIterations = -1;

		virtual void ExecuteTask(const TSharedPtr<FTaskManager>& TaskManager) override;
	};

	// Main thread execution
	class PCGEXTENDEDTOOLKIT_API IExecuteOnMainThread : public IAsyncHandle
	{
	public:
		FCompletionCallback OnCompleteCallback;

		explicit IExecuteOnMainThread();
		virtual bool Start() override;

	protected:
		double EndTime = 0.0;
		virtual void Schedule();
		virtual bool Execute();
		virtual void OnEnd(bool bWasCancelled) override;
		bool ShouldStop();
	};

	class PCGEXTENDEDTOOLKIT_API FTimeSlicedMainThreadLoop : public IExecuteOnMainThread
	{
	protected:
		FScope Scope = FScope{};

	public:
		using FIterationCallback = std::function<void(const int32, const FScope&)>;
		FIterationCallback OnIterationCallback;

		explicit FTimeSlicedMainThreadLoop(const int32 NumIterations);
		virtual bool Start() override;
		virtual void Cancel() override;

	protected:
		virtual bool Execute() override;
	};
}
