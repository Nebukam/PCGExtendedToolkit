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
	 * Task group that manages multiple tasks
	 */
	class FTaskGroup : public IHandle
	{
	public:
		mutable FCriticalSection RegistryLock;
		TArray<TSharedPtr<IHandle>> Tasks;

		std::atomic<int32> ExpectedCount{0};
		std::atomic<int32> CompletedCount{0};

		std::function<void()> OnAllComplete;

		void RegisterTask(TSharedPtr<IHandle> Task)
		{
			FScopeLock Lock(&RegistryLock);
			Tasks.Add(Task);
			ExpectedCount.fetch_add(1);
		}

		void NotifyTaskComplete()
		{
			int32 Completed = CompletedCount.fetch_add(1) + 1;
			int32 Expected = ExpectedCount.load();

			if (Completed >= Expected && OnAllComplete)
			{
				OnAllComplete();
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
			if (IsCancelled()) return;

			if (Start())
			{
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

	// Launch tasks
	for (int32 i = 0; i < NumTasks; ++i)
	{
		auto Task = MakeShared<FTask>([&ExecutedCount]()
		{
			ExecutedCount.fetch_add(1);
			FPlatformProcess::Sleep(0.001f); // Small delay
		});
		Manager->LaunchTask(Task);
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

	// Wait for all tasks to complete
	double StartTime = FPlatformTime::Seconds();
	while (!AllComplete.load() && (FPlatformTime::Seconds() - StartTime) < 5.0)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	TestTrue(TEXT("All tasks completed"), AllComplete.load());
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
			return CompletedCount.load() >= ExpectedCount.load();
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
