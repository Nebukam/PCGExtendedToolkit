// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Task Manager Tests
 *
 * Tests for the FTaskManager, FTaskGroup, and async handle patterns.
 * These tests verify the task scheduling and completion mechanisms.
 *
 * Test naming: PCGEx.Functional.Threading.TaskManager.<Scenario>
 */

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"

#include <atomic>
#include <functional>

// =============================================================================
// Mock Task System (simulates PCGExMT patterns)
// =============================================================================

namespace MockTaskSystem
{
	class IHandle;
	class FTaskGroup;
	class FTaskManager;

	/**
	 * Base handle for async operations
	 */
	class IHandle : public TSharedFromThis<IHandle>
	{
	public:
		enum class EState : uint8
		{
			Idle = 0,
			Running = 1,
			Ended = 2
		};

		std::atomic<bool> bCancelled{false};
		std::atomic<EState> State{EState::Idle};

		TWeakPtr<FTaskGroup> ParentGroup;
		std::function<void()> OnComplete;

		virtual ~IHandle() = default;

		bool IsCancelled() const { return bCancelled.load(std::memory_order_acquire); }
		EState GetState() const { return State.load(std::memory_order_acquire); }

		virtual bool Start()
		{
			EState Expected = EState::Idle;
			return State.compare_exchange_strong(Expected, EState::Running);
		}

		virtual void Cancel()
		{
			bCancelled.store(true, std::memory_order_release);
		}

		virtual void Complete()
		{
			EState Expected = EState::Running;
			if (State.compare_exchange_strong(Expected, EState::Ended))
			{
				if (OnComplete) OnComplete();
			}
		}
	};

	/**
	 * Task group that manages multiple tasks.
	 * Mirrors production IAsyncHandleGroup: PendingRegistrations suppresses
	 * premature completion, StartedCount adds a second consistency check,
	 * and CAS ensures the callback fires exactly once.
	 */
	class FTaskGroup : public IHandle
	{
	public:
		mutable FCriticalSection RegistryLock;
		TArray<TSharedPtr<IHandle>> Tasks;

		std::atomic<int32> PendingRegistrations{0};
		std::atomic<int32> ExpectedCount{0};
		std::atomic<int32> StartedCount{0};
		std::atomic<int32> CompletedCount{0};
		std::atomic<bool> bCompletionFired{false};

		std::function<void()> OnAllComplete;

		// RAII guard that blocks CheckCompletion during batch registration.
		// Callers that register multiple tasks must hold a guard for the
		// duration of the batch so that a fast-completing task cannot trigger
		// group completion before all siblings are registered.
		struct FRegistrationGuard
		{
			TSharedPtr<FTaskGroup> Parent;

			explicit FRegistrationGuard(const TSharedPtr<FTaskGroup>& InParent)
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

		void RegisterTask(TSharedPtr<IHandle> Task)
		{
			FScopeLock Lock(&RegistryLock);
			Tasks.Add(Task);
			ExpectedCount.fetch_add(1, std::memory_order_acq_rel);
		}

		void NotifyTaskStarted()
		{
			StartedCount.fetch_add(1, std::memory_order_acq_rel);
		}

		void NotifyTaskComplete()
		{
			CompletedCount.fetch_add(1, std::memory_order_acq_rel);
			CheckCompletion();
		}

		void CheckCompletion()
		{
			// While any thread is still registering tasks, suppress completion.
			if (PendingRegistrations.load(std::memory_order_acquire) > 0) { return; }

			std::atomic_thread_fence(std::memory_order_seq_cst);

			const int32 Completed = CompletedCount.load(std::memory_order_acquire);
			const int32 Expected = ExpectedCount.load(std::memory_order_acquire);
			const int32 Started = StartedCount.load(std::memory_order_acquire);

			if (Completed >= Expected && Completed == Started && Expected > 0)
			{
				// CAS ensures exactly one thread fires the callback.
				bool bExpectedFalse = false;
				if (bCompletionFired.compare_exchange_strong(bExpectedFalse, true))
				{
					if (OnAllComplete) { OnAllComplete(); }
				}
			}
		}

		virtual void Cancel() override
		{
			IHandle::Cancel();
			FScopeLock Lock(&RegistryLock);
			for (auto& Task : Tasks)
			{
				Task->Cancel();
			}
		}

		int32 GetCompletedCount() const { return CompletedCount.load(); }
		int32 GetExpectedCount() const { return ExpectedCount.load(); }
	};

	/**
	 * Simple task that executes a callback
	 */
	class FTask : public IHandle
	{
	public:
		std::function<void()> Work;
		TWeakPtr<FTaskGroup> Group;

		FTask(std::function<void()> InWork) : Work(InWork) {}

		void Execute()
		{
			if (IsCancelled())
			{
				// Mirror production: cancelled tasks still notify the group
				// so completion tracking stays consistent (Expected == Completed).
				// Without this, cancelled tasks silently disappear and
				// OnAllComplete can never fire.
				if (auto G = Group.Pin())
				{
					G->NotifyTaskStarted();
					G->NotifyTaskComplete();
				}
				return;
			}

			if (Start())
			{
				if (auto G = Group.Pin())
				{
					G->NotifyTaskStarted();
				}

				if (Work) Work();
				Complete();

				if (auto G = Group.Pin())
				{
					G->NotifyTaskComplete();
				}
			}
		}
	};

	/**
	 * Task manager (root of task hierarchy)
	 */
	class FTaskManager : public FTaskGroup
	{
	public:
		std::atomic<bool> bWaitingForTasks{false};

		std::function<void(bool)> OnEndCallback;

		void LaunchTask(TSharedPtr<FTask> Task)
		{
			if (IsCancelled()) return;

			Task->Group = StaticCastSharedRef<FTaskGroup>(AsShared());
			RegisterTask(Task);

			// Simulate async execution
			Async(EAsyncExecution::ThreadPool, [Task]()
			{
				Task->Execute();
			});
		}

		bool IsWaitingForTasks() const
		{
			return GetCompletedCount() < GetExpectedCount();
		}
	};
}

// =============================================================================
// Task Group Tests
// =============================================================================

/**
 * Test task group completion tracking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTaskGroupCompletionTest,
	"PCGEx.Functional.Threading.TaskManager.GroupCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTaskGroupCompletionTest::RunTest(const FString& Parameters)
{
	using namespace MockTaskSystem;

	auto Manager = MakeShared<FTaskManager>();
	std::atomic<bool> AllComplete{false};
	std::atomic<int32> ExecutedCount{0};

	Manager->OnAllComplete = [&AllComplete]()
	{
		AllComplete.store(true);
	};

	const int32 NumTasks = 10;

	// Launch tasks within a registration guard so that fast-completing tasks
	// cannot trigger group completion before all siblings are registered.
	{
		FTaskGroup::FRegistrationGuard Guard(Manager);
		for (int32 i = 0; i < NumTasks; ++i)
		{
			auto Task = MakeShared<FTask>([&ExecutedCount]()
			{
				ExecutedCount.fetch_add(1);
				FPlatformProcess::Sleep(0.001f); // Small delay
			});
			Manager->LaunchTask(Task);
		}
	}

	// Wait for completion
	double StartTime = FPlatformTime::Seconds();
	while (!AllComplete.load() && (FPlatformTime::Seconds() - StartTime) < 5.0)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	TestTrue(TEXT("All tasks completed"), AllComplete.load());
	TestEqual(TEXT("All tasks executed"), ExecutedCount.load(), NumTasks);
	TestEqual(TEXT("Completed count matches"), Manager->GetCompletedCount(), NumTasks);

	return true;
}

/**
 * Test task group cancellation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTaskGroupCancellationTest,
	"PCGEx.Functional.Threading.TaskManager.GroupCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTaskGroupCancellationTest::RunTest(const FString& Parameters)
{
	using namespace MockTaskSystem;

	const int32 NumIterations = 20;
	int32 CleanCancellations = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		auto Manager = MakeShared<FTaskManager>();
		std::atomic<int32> ExecutedCount{0};
		std::atomic<int32> CancelledCount{0};

		const int32 NumTasks = 50;

		// Launch tasks
		for (int32 i = 0; i < NumTasks; ++i)
		{
			auto Task = MakeShared<FTask>([&ExecutedCount]()
			{
				FPlatformProcess::Sleep(0.01f); // Longer delay to allow cancellation
				ExecutedCount.fetch_add(1);
			});
			Manager->LaunchTask(Task);
		}

		// Cancel after small delay
		FPlatformProcess::Sleep(0.005f);
		Manager->Cancel();

		// Wait for tasks to finish
		FPlatformProcess::Sleep(0.1f);

		// Some tasks should have been cancelled
		int32 Executed = ExecutedCount.load();
		if (Executed < NumTasks)
		{
			CleanCancellations++;
		}
	}

	AddInfo(FString::Printf(TEXT("Successful cancellations: %d / %d"), CleanCancellations, NumIterations));
	// We expect at least some cancellations to work
	TestTrue(TEXT("Some cancellations should succeed"), CleanCancellations > 0);

	return true;
}

// =============================================================================
// Task Execution Order Tests
// =============================================================================

/**
 * Test that tasks with dependencies execute in correct order
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTaskDependencyOrderTest,
	"PCGEx.Functional.Threading.TaskManager.DependencyOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTaskDependencyOrderTest::RunTest(const FString& Parameters)
{
	// Simulate dependency chain: A -> B -> C
	// C depends on B, B depends on A

	std::atomic<int32> ExecutionOrder{0};
	std::atomic<int32> AOrder{-1};
	std::atomic<int32> BOrder{-1};
	std::atomic<int32> COrder{-1};

	std::atomic<bool> AComplete{false};
	std::atomic<bool> BComplete{false};

	// Task A
	auto TaskA = Async(EAsyncExecution::ThreadPool, [&]()
	{
		AOrder.store(ExecutionOrder.fetch_add(1));
		FPlatformProcess::Sleep(0.01f);
		AComplete.store(true);
	});

	// Task B (waits for A)
	auto TaskB = Async(EAsyncExecution::ThreadPool, [&]()
	{
		// Wait for A
		while (!AComplete.load()) { FPlatformProcess::YieldThread(); }

		BOrder.store(ExecutionOrder.fetch_add(1));
		FPlatformProcess::Sleep(0.01f);
		BComplete.store(true);
	});

	// Task C (waits for B)
	auto TaskC = Async(EAsyncExecution::ThreadPool, [&]()
	{
		// Wait for B
		while (!BComplete.load()) { FPlatformProcess::YieldThread(); }

		COrder.store(ExecutionOrder.fetch_add(1));
	});

	TaskA.Wait();
	TaskB.Wait();
	TaskC.Wait();

	TestTrue(TEXT("A executes before B"), AOrder.load() < BOrder.load());
	TestTrue(TEXT("B executes before C"), BOrder.load() < COrder.load());

	return true;
}

// =============================================================================
// Concurrent Task Launch Tests
// =============================================================================

/**
 * Test launching many tasks concurrently
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConcurrentTaskLaunchTest,
	"PCGEx.Functional.Threading.TaskManager.ConcurrentLaunch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConcurrentTaskLaunchTest::RunTest(const FString& Parameters)
{
	using namespace MockTaskSystem;

	auto Manager = MakeShared<FTaskManager>();
	std::atomic<int32> ExecutedCount{0};
	std::atomic<bool> AllComplete{false};

	Manager->OnAllComplete = [&AllComplete]()
	{
		AllComplete.store(true);
	};

	const int32 NumTasks = 100;
	std::atomic<bool> StartFlag{false};

	// Multiple threads launch tasks simultaneously
	TArray<TFuture<void>> LaunchFutures;
	const int32 NumLaunchers = 4;

	for (int32 L = 0; L < NumLaunchers; ++L)
	{
		LaunchFutures.Add(Async(EAsyncExecution::ThreadPool, [&, L]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Each launcher thread holds a registration guard for its batch.
			// This prevents premature completion: CheckCompletion is suppressed
			// while any guard is alive, matching production FRegistrationGuard.
			FTaskGroup::FRegistrationGuard Guard(Manager);
			for (int32 i = 0; i < NumTasks / NumLaunchers; ++i)
			{
				auto Task = MakeShared<FTask>([&ExecutedCount]()
				{
					ExecutedCount.fetch_add(1);
				});
				Manager->LaunchTask(Task);
			}
		}));
	}

	StartFlag.store(true);

	for (auto& F : LaunchFutures)
	{
		F.Wait();
	}

	// Wait for completion callback — FRegistrationGuard ensures it fires only
	// after all launcher threads finish registration and all tasks complete.
	double StartTime = FPlatformTime::Seconds();
	while (!AllComplete.load() && (FPlatformTime::Seconds() - StartTime) < 5.0)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	TestTrue(TEXT("Completion callback fired"), AllComplete.load());
	TestEqual(TEXT("All tasks executed"), ExecutedCount.load(), NumTasks);

	return true;
}

// =============================================================================
// Task Manager Reset Tests
// =============================================================================

/**
 * Test resetting task manager for reuse
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTaskManagerResetTest,
	"PCGEx.Functional.Threading.TaskManager.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTaskManagerResetTest::RunTest(const FString& Parameters)
{
	struct FResettableManager
	{
		std::atomic<int32> CompletedCount{0};
		std::atomic<int32> ExpectedCount{0};
		std::atomic<bool> bCancelled{false};

		void Reset()
		{
			CompletedCount.store(0);
			ExpectedCount.store(0);
			bCancelled.store(false);
		}

		void AddExpected(int32 Count = 1)
		{
			ExpectedCount.fetch_add(Count);
		}

		void NotifyComplete()
		{
			CompletedCount.fetch_add(1);
		}

		bool IsComplete() const
		{
			int32 Expected = ExpectedCount.load();
			// Not complete if nothing is expected (not started)
			if (Expected == 0) return false;
			return CompletedCount.load() >= Expected;
		}
	};

	FResettableManager Manager;

	// First run
	Manager.AddExpected(5);
	for (int32 i = 0; i < 5; ++i)
	{
		Manager.NotifyComplete();
	}
	TestTrue(TEXT("First run complete"), Manager.IsComplete());
	TestEqual(TEXT("First run count"), Manager.CompletedCount.load(), 5);

	// Reset
	Manager.Reset();
	TestEqual(TEXT("Reset completed count"), Manager.CompletedCount.load(), 0);
	TestEqual(TEXT("Reset expected count"), Manager.ExpectedCount.load(), 0);
	TestFalse(TEXT("Not complete after reset"), Manager.IsComplete());

	// Second run
	Manager.AddExpected(3);
	for (int32 i = 0; i < 3; ++i)
	{
		Manager.NotifyComplete();
	}
	TestTrue(TEXT("Second run complete"), Manager.IsComplete());
	TestEqual(TEXT("Second run count"), Manager.CompletedCount.load(), 3);

	return true;
}

// =============================================================================
// Callback Safety Tests
// =============================================================================

/**
 * Test that callbacks are only invoked once
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCallbackSafetyTest,
	"PCGEx.Functional.Threading.TaskManager.CallbackSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCallbackSafetyTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 100;
	int32 MultipleCallbacks = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		std::atomic<int32> CallbackCount{0};
		std::atomic<bool> StartFlag{false};

		// Simulated completion mechanism
		struct FCompletionHandler
		{
			std::atomic<bool> bCompleted{false};
			std::function<void()> Callback;

			bool TryComplete()
			{
				bool Expected = false;
				if (bCompleted.compare_exchange_strong(Expected, true))
				{
					if (Callback) Callback();
					return true;
				}
				return false;
			}
		};

		FCompletionHandler Handler;
		Handler.Callback = [&CallbackCount]()
		{
			CallbackCount.fetch_add(1);
		};

		// Multiple threads try to complete
		TArray<TFuture<void>> Futures;
		for (int32 t = 0; t < 8; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&Handler, &StartFlag]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
				Handler.TryComplete();
			}));
		}

		StartFlag.store(true);

		for (auto& F : Futures)
		{
			F.Wait();
		}

		if (CallbackCount.load() != 1)
		{
			MultipleCallbacks++;
		}
	}

	TestEqual(TEXT("Callback invoked exactly once"), MultipleCallbacks, 0);

	return true;
}

// =============================================================================
// Work Stealing Pattern Tests
// =============================================================================

/**
 * Test work distribution across threads
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWorkDistributionTest,
	"PCGEx.Functional.Threading.TaskManager.WorkDistribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWorkDistributionTest::RunTest(const FString& Parameters)
{
	const int32 TotalWork = 1000;
	const int32 NumWorkers = 4;

	std::atomic<int32> WorkIndex{0};
	TArray<std::atomic<int32>> WorkPerThread;
	WorkPerThread.SetNum(NumWorkers);
	for (auto& W : WorkPerThread) W.store(0);

	TArray<TFuture<void>> Workers;
	for (int32 W = 0; W < NumWorkers; ++W)
	{
		Workers.Add(Async(EAsyncExecution::ThreadPool, [&, W]()
		{
			while (true)
			{
				int32 MyWork = WorkIndex.fetch_add(1);
				if (MyWork >= TotalWork) break;

				// Simulate work
				volatile int32 Sum = 0;
				for (int32 i = 0; i < 100; ++i) Sum += i;

				WorkPerThread[W].fetch_add(1);
			}
		}));
	}

	for (auto& F : Workers)
	{
		F.Wait();
	}

	// All work should be done
	int32 TotalDone = 0;
	for (int32 W = 0; W < NumWorkers; ++W)
	{
		int32 Done = WorkPerThread[W].load();
		TotalDone += Done;
		AddInfo(FString::Printf(TEXT("Worker %d: %d items"), W, Done));
	}

	TestEqual(TEXT("All work completed"), TotalDone, TotalWork);

	// Check distribution (should be somewhat balanced)
	int32 MinWork = INT32_MAX;
	int32 MaxWork = 0;
	for (int32 W = 0; W < NumWorkers; ++W)
	{
		int32 Done = WorkPerThread[W].load();
		MinWork = FMath::Min(MinWork, Done);
		MaxWork = FMath::Max(MaxWork, Done);
	}

	AddInfo(FString::Printf(TEXT("Work range: %d - %d"), MinWork, MaxWork));

	return true;
}

// =============================================================================
// Registration Guard Suppression Tests
// =============================================================================

/**
 * Test that FRegistrationGuard suppresses premature completion.
 * All tasks complete while the guard is held — callback must NOT fire
 * until the guard destructs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRegistrationGuardSuppressionTest,
	"PCGEx.Functional.Threading.TaskManager.RegistrationGuard.Suppression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRegistrationGuardSuppressionTest::RunTest(const FString& Parameters)
{
	using namespace MockTaskSystem;

	auto Manager = MakeShared<FTaskManager>();
	std::atomic<bool> AllComplete{false};
	std::atomic<int32> ExecutedCount{0};

	Manager->OnAllComplete = [&AllComplete]()
	{
		AllComplete.store(true);
	};

	const int32 NumTasks = 10;

	{
		FTaskGroup::FRegistrationGuard Guard(Manager);

		for (int32 i = 0; i < NumTasks; ++i)
		{
			auto Task = MakeShared<FTask>([&ExecutedCount]()
			{
				ExecutedCount.fetch_add(1);
			});
			Manager->LaunchTask(Task);
		}

		// Wait for all tasks to finish executing (no sleep in tasks, so fast)
		double WaitStart = FPlatformTime::Seconds();
		while (ExecutedCount.load() < NumTasks && (FPlatformTime::Seconds() - WaitStart) < 5.0)
		{
			FPlatformProcess::Sleep(0.01f);
		}

		// All tasks executed but guard is still held — completion MUST be suppressed
		TestEqual(TEXT("All tasks executed while guard held"), ExecutedCount.load(), NumTasks);
		TestFalse(TEXT("Completion suppressed while guard held"), AllComplete.load());
	}
	// Guard released here — CheckCompletion runs and fires callback

	// Brief wait for CheckCompletion to propagate
	FPlatformProcess::Sleep(0.01f);

	TestTrue(TEXT("Completion fires after guard release"), AllComplete.load());

	return true;
}

// =============================================================================
// Cancelled Task Completion Tests
// =============================================================================

/**
 * Test that cancelled tasks still notify the group.
 * In production, cancelled tasks call OnEnd → NotifyCompleted so the group
 * can still reach completion. Without this, Expected > Completed forever
 * and OnAllComplete never fires.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCancelledTaskCompletionTest,
	"PCGEx.Functional.Threading.TaskManager.CancelledTaskCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCancelledTaskCompletionTest::RunTest(const FString& Parameters)
{
	using namespace MockTaskSystem;

	auto Manager = MakeShared<FTaskManager>();
	std::atomic<bool> AllComplete{false};
	std::atomic<int32> ExecutedCount{0};

	Manager->OnAllComplete = [&AllComplete]()
	{
		AllComplete.store(true);
	};

	const int32 NumTasks = 12;
	int32 NumCancelled = 0;

	{
		FTaskGroup::FRegistrationGuard Guard(Manager);
		for (int32 i = 0; i < NumTasks; ++i)
		{
			auto Task = MakeShared<FTask>([&ExecutedCount]()
			{
				ExecutedCount.fetch_add(1);
			});

			// Cancel every 3rd task before launch
			if (i % 3 == 0)
			{
				Task->Cancel();
				NumCancelled++;
			}

			Manager->LaunchTask(Task);
		}
	}

	// Wait for completion
	double StartTime = FPlatformTime::Seconds();
	while (!AllComplete.load() && (FPlatformTime::Seconds() - StartTime) < 5.0)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	TestTrue(TEXT("Completion fires despite cancelled tasks"), AllComplete.load());
	TestEqual(TEXT("Only non-cancelled tasks executed work"), ExecutedCount.load(), NumTasks - NumCancelled);
	TestEqual(TEXT("All tasks counted as completed"), Manager->GetCompletedCount(), NumTasks);

	return true;
}
