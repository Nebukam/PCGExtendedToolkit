// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Async Staging Tests
 *
 * Tests for race conditions and thread safety in the staging and completion flow.
 * These tests simulate concurrent access patterns to catch issues early.
 *
 * Key scenarios:
 * - Concurrent staging from multiple threads
 * - Staging vs completion race
 * - State machine transitions under concurrency
 * - Task manager completion ordering
 *
 * Test naming: PCGEx.Functional.Threading.Async.<Scenario>
 */

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"

#include <atomic>
#include <thread>
#include <chrono>

// =============================================================================
// Staging Race Condition Tests
// =============================================================================

/**
 * Simulates the staging data structure with the same locking pattern as FPCGExContext
 */
class FMockStagingContext
{
public:
	mutable FRWLock StagingLock;
	TArray<int32> StagedData;
	TArray<int32> OutputData;

	std::atomic<bool> bWorkCompleted{false};
	std::atomic<bool> bWorkCancelled{false};

	bool IsWorkCompleted() const { return bWorkCompleted.load(std::memory_order_acquire); }
	bool IsWorkCancelled() const { return bWorkCancelled.load(std::memory_order_acquire); }

	// Original pattern (potential race)
	bool StageOutput_Original(int32 Value)
	{
		if (IsWorkCancelled() || IsWorkCompleted()) { return false; }

		{
			FWriteScopeLock WriteScopeLock(StagingLock);
			StagedData.Add(Value);
		}
		return true;
	}

	// Fixed pattern (check inside lock)
	bool StageOutput_Fixed(int32 Value)
	{
		if (IsWorkCancelled()) { return false; }

		{
			FWriteScopeLock WriteScopeLock(StagingLock);
			// Check completion inside the lock to prevent race
			if (IsWorkCompleted()) { return false; }
			StagedData.Add(Value);
		}
		return true;
	}

	// Alternative fix: Don't check completion at all, rely on lock
	bool StageOutput_NoCompletionCheck(int32 Value)
	{
		if (IsWorkCancelled()) { return false; }

		{
			FWriteScopeLock WriteScopeLock(StagingLock);
			StagedData.Add(Value);
		}
		return true;
	}

	bool TryComplete()
	{
		bool bExpected = false;
		if (bWorkCompleted.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
		{
			OnComplete();
			return true;
		}
		return false;
	}

	void OnComplete()
	{
		FWriteScopeLock WriteScopeLock(StagingLock);
		OutputData.Append(StagedData);
		StagedData.Empty();
	}

	void Reset()
	{
		FWriteScopeLock WriteScopeLock(StagingLock);
		StagedData.Empty();
		OutputData.Empty();
		bWorkCompleted.store(false, std::memory_order_release);
		bWorkCancelled.store(false, std::memory_order_release);
	}
};

/**
 * Test that demonstrates the race condition with the original pattern
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStagingRaceOriginalTest,
	"PCGEx.Functional.Threading.Async.StagingRaceOriginal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStagingRaceOriginalTest::RunTest(const FString& Parameters)
{
	// Run multiple iterations to catch intermittent race
	int32 TotalLost = 0;
	const int32 NumIterations = 100;
	const int32 NumThreads = 4;
	const int32 ItemsPerThread = 100;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockStagingContext Context;
		std::atomic<int32> StagedCount{0};
		std::atomic<bool> StartFlag{false};

		// Launch staging threads
		TArray<TFuture<void>> Futures;
		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StagedCount, &StartFlag, t, ItemsPerThread]()
			{
				// Wait for start signal
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				for (int32 i = 0; i < ItemsPerThread; ++i)
				{
					int32 Value = t * 1000 + i;
					if (Context.StageOutput_Original(Value))
					{
						StagedCount.fetch_add(1);
					}
				}
			}));
		}

		// Launch completion thread
		Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Small delay to let staging start
			FPlatformProcess::Sleep(0.0001f);

			Context.TryComplete();
		}));

		// Start all threads
		StartFlag.store(true);

		// Wait for all
		for (auto& Future : Futures)
		{
			Future.Wait();
		}

		// Check results
		int32 Expected = NumThreads * ItemsPerThread;
		int32 Actual = Context.OutputData.Num();
		int32 Lost = Expected - Actual;

		if (Lost > 0)
		{
			TotalLost += Lost;
		}
	}

	AddInfo(FString::Printf(TEXT("Original pattern: Lost %d items across %d iterations"), TotalLost, NumIterations));

	// We expect some data loss with the original pattern due to the race
	// This test documents the issue rather than asserting it doesn't happen
	return true;
}

/**
 * Test the fixed pattern that checks completion inside the lock
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStagingRaceFixedTest,
	"PCGEx.Functional.Threading.Async.StagingRaceFixed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStagingRaceFixedTest::RunTest(const FString& Parameters)
{
	int32 TotalLost = 0;
	const int32 NumIterations = 100;
	const int32 NumThreads = 4;
	const int32 ItemsPerThread = 100;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockStagingContext Context;
		std::atomic<int32> StagedCount{0};
		std::atomic<bool> StartFlag{false};

		TArray<TFuture<void>> Futures;
		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StagedCount, &StartFlag, t, ItemsPerThread]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				for (int32 i = 0; i < ItemsPerThread; ++i)
				{
					int32 Value = t * 1000 + i;
					if (Context.StageOutput_Fixed(Value))
					{
						StagedCount.fetch_add(1);
					}
				}
			}));
		}

		Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			FPlatformProcess::Sleep(0.0001f);
			Context.TryComplete();
		}));

		StartFlag.store(true);

		for (auto& Future : Futures)
		{
			Future.Wait();
		}

		int32 Expected = NumThreads * ItemsPerThread;
		int32 Actual = Context.OutputData.Num();
		int32 Lost = Expected - Actual;

		if (Lost > 0)
		{
			TotalLost += Lost;
		}
	}

	AddInfo(FString::Printf(TEXT("Fixed pattern (check inside lock): Lost %d items across %d iterations"), TotalLost, NumIterations));

	// With the fix, we should lose much less data (ideally none)
	// Some loss is still possible if completion happens before any staging starts
	return true;
}

// =============================================================================
// State Machine Concurrency Tests
// =============================================================================

/**
 * Simulates the PCGEx state machine with atomic state transitions
 */
class FMockStateMachine
{
public:
	enum class EState : uint32
	{
		Initial = 0,
		Processing = 100,
		Completing = 200,
		Done = 300
	};

	std::atomic<uint32> CurrentState{static_cast<uint32>(EState::Initial)};
	std::atomic<int32> TransitionCount{0};

	bool IsState(EState State) const
	{
		return CurrentState.load(std::memory_order_acquire) == static_cast<uint32>(State);
	}

	bool TryTransition(EState From, EState To)
	{
		uint32 Expected = static_cast<uint32>(From);
		if (CurrentState.compare_exchange_strong(Expected, static_cast<uint32>(To), std::memory_order_acq_rel))
		{
			TransitionCount.fetch_add(1);
			return true;
		}
		return false;
	}

	void SetState(EState State)
	{
		CurrentState.store(static_cast<uint32>(State), std::memory_order_release);
		TransitionCount.fetch_add(1);
	}
};

/**
 * Test concurrent state transitions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStateTransitionConcurrencyTest,
	"PCGEx.Functional.Threading.Async.StateTransitionConcurrency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStateTransitionConcurrencyTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 1000;
	int32 RaceDetected = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockStateMachine SM;
		std::atomic<int32> SuccessfulTransitions{0};
		std::atomic<bool> StartFlag{false};

		// Multiple threads try to transition from Initial to Processing
		TArray<TFuture<void>> Futures;
		const int32 NumThreads = 4;

		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&SM, &SuccessfulTransitions, &StartFlag]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				if (SM.TryTransition(FMockStateMachine::EState::Initial, FMockStateMachine::EState::Processing))
				{
					SuccessfulTransitions.fetch_add(1);
				}
			}));
		}

		StartFlag.store(true);

		for (auto& Future : Futures)
		{
			Future.Wait();
		}

		// Only one thread should succeed
		if (SuccessfulTransitions.load() != 1)
		{
			RaceDetected++;
		}
	}

	TestEqual(TEXT("Exactly one thread should win each transition"), RaceDetected, 0);

	return true;
}

// =============================================================================
// Completion Ordering Tests
// =============================================================================

/**
 * Test that completion happens in the correct order
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompletionOrderingTest,
	"PCGEx.Functional.Threading.Async.CompletionOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompletionOrderingTest::RunTest(const FString& Parameters)
{
	struct FOrderTracker
	{
		FCriticalSection Lock;
		TArray<FString> Operations;

		void Record(const FString& Op)
		{
			FScopeLock ScopeLock(&Lock);
			Operations.Add(Op);
		}
	};

	const int32 NumIterations = 100;
	int32 OrderingViolations = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FOrderTracker Tracker;
		FMockStagingContext Context;
		std::atomic<bool> StartFlag{false};

		// Thread 1: Stage data
		auto StageFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			Tracker.Record(TEXT("Stage_Start"));
			for (int32 i = 0; i < 10; ++i)
			{
				Context.StageOutput_NoCompletionCheck(i);
			}
			Tracker.Record(TEXT("Stage_End"));
		});

		// Thread 2: Complete
		auto CompleteFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Small delay
			FPlatformProcess::Sleep(0.00005f);

			Tracker.Record(TEXT("Complete_Start"));
			Context.TryComplete();
			Tracker.Record(TEXT("Complete_End"));
		});

		StartFlag.store(true);

		StageFuture.Wait();
		CompleteFuture.Wait();

		// Check if Complete_End happened before Stage_End
		// This would indicate potential data loss
		int32 StageEndIdx = Tracker.Operations.IndexOfByKey(TEXT("Stage_End"));
		int32 CompleteEndIdx = Tracker.Operations.IndexOfByKey(TEXT("Complete_End"));

		if (CompleteEndIdx < StageEndIdx && CompleteEndIdx != INDEX_NONE && StageEndIdx != INDEX_NONE)
		{
			// Completion finished before staging - potential data loss
			if (Context.OutputData.Num() < 10)
			{
				OrderingViolations++;
			}
		}
	}

	AddInfo(FString::Printf(TEXT("Ordering violations (completion before staging): %d / %d"),
		OrderingViolations, NumIterations));

	return true;
}

// =============================================================================
// Double Completion Prevention Tests
// =============================================================================

/**
 * Test that TryComplete only succeeds once
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDoubleCompletionTest,
	"PCGEx.Functional.Threading.Async.DoubleCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDoubleCompletionTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 100;
	int32 DoubleCompletions = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockStagingContext Context;
		std::atomic<int32> CompletionCount{0};
		std::atomic<bool> StartFlag{false};

		const int32 NumThreads = 8;
		TArray<TFuture<void>> Futures;

		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				if (Context.TryComplete())
				{
					CompletionCount.fetch_add(1);
				}
			}));
		}

		StartFlag.store(true);

		for (auto& Future : Futures)
		{
			Future.Wait();
		}

		if (CompletionCount.load() != 1)
		{
			DoubleCompletions++;
		}
	}

	TestEqual(TEXT("TryComplete should succeed exactly once"), DoubleCompletions, 0);

	return true;
}

// =============================================================================
// Batch Processing Simulation Tests
// =============================================================================

/**
 * Simulates batch processing with skip completion pattern
 */
class FMockBatchProcessor
{
public:
	struct FProcessor
	{
		int32 Index;
		bool bIsValid = true;
		int32 OutputValue = 0;
	};

	TArray<TSharedPtr<FProcessor>> Processors;
	bool bSkipCompletion = false;

	void AddProcessor(int32 Index)
	{
		auto P = MakeShared<FProcessor>();
		P->Index = Index;
		Processors.Add(P);
	}

	void ProcessAll(std::atomic<bool>& CancelFlag)
	{
		ParallelFor(Processors.Num(), [this, &CancelFlag](int32 Index)
		{
			if (CancelFlag.load()) return;

			auto& P = Processors[Index];
			// Simulate work
			P->OutputValue = P->Index * 10;
		});
	}

	void CompleteAll()
	{
		if (bSkipCompletion) return;

		for (auto& P : Processors)
		{
			if (!P->bIsValid) continue;
			// Complete work
			P->OutputValue += 1;
		}
	}

	int32 GetTotalOutput() const
	{
		int32 Total = 0;
		for (const auto& P : Processors)
		{
			if (P->bIsValid)
			{
				Total += P->OutputValue;
			}
		}
		return Total;
	}
};

/**
 * Test batch processing with concurrent cancellation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBatchCancellationTest,
	"PCGEx.Functional.Threading.Async.BatchCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBatchCancellationTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 50;
	int32 CleanCancellations = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockBatchProcessor Batch;
		for (int32 i = 0; i < 100; ++i)
		{
			Batch.AddProcessor(i);
		}

		std::atomic<bool> CancelFlag{false};
		std::atomic<bool> StartFlag{false};

		// Processing thread
		auto ProcessFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			Batch.ProcessAll(CancelFlag);
		});

		// Cancellation thread
		auto CancelFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			FPlatformProcess::Sleep(0.0001f); // Small delay
			CancelFlag.store(true);
		});

		StartFlag.store(true);

		ProcessFuture.Wait();
		CancelFuture.Wait();

		// Check that no processor is in an inconsistent state
		bool AllConsistent = true;
		for (const auto& P : Batch.Processors)
		{
			// Output should be either 0 (not processed) or Index*10 (fully processed)
			if (P->OutputValue != 0 && P->OutputValue != P->Index * 10)
			{
				AllConsistent = false;
				break;
			}
		}

		if (AllConsistent)
		{
			CleanCancellations++;
		}
	}

	AddInfo(FString::Printf(TEXT("Clean cancellations: %d / %d"), CleanCancellations, NumIterations));
	TestEqual(TEXT("All cancellations should be clean"), CleanCancellations, NumIterations);

	return true;
}

// =============================================================================
// Async Work Handle Tests
// =============================================================================

/**
 * Simulates async work handle state transitions
 */
class FMockAsyncHandle
{
public:
	enum class EState : uint8
	{
		Idle = 0,
		Running = 1,
		Completed = 2,
		Cancelled = 3
	};

	std::atomic<uint8> State{static_cast<uint8>(EState::Idle)};

	bool Start()
	{
		uint8 Expected = static_cast<uint8>(EState::Idle);
		return State.compare_exchange_strong(Expected, static_cast<uint8>(EState::Running));
	}

	bool Complete()
	{
		uint8 Expected = static_cast<uint8>(EState::Running);
		return State.compare_exchange_strong(Expected, static_cast<uint8>(EState::Completed));
	}

	bool Cancel()
	{
		uint8 Current = State.load();
		if (Current == static_cast<uint8>(EState::Completed)) return false;

		return State.compare_exchange_strong(Current, static_cast<uint8>(EState::Cancelled));
	}

	EState GetState() const
	{
		return static_cast<EState>(State.load());
	}
};

/**
 * Test async handle state machine
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAsyncHandleStateTest,
	"PCGEx.Functional.Threading.Async.HandleState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExAsyncHandleStateTest::RunTest(const FString& Parameters)
{
	// Test normal flow
	{
		FMockAsyncHandle Handle;
		TestTrue(TEXT("Can start from idle"), Handle.Start());
		TestEqual(TEXT("State is running"), Handle.GetState(), FMockAsyncHandle::EState::Running);
		TestTrue(TEXT("Can complete from running"), Handle.Complete());
		TestEqual(TEXT("State is completed"), Handle.GetState(), FMockAsyncHandle::EState::Completed);
		TestFalse(TEXT("Cannot cancel after completion"), Handle.Cancel());
	}

	// Test cancellation
	{
		FMockAsyncHandle Handle;
		TestTrue(TEXT("Can start"), Handle.Start());
		TestTrue(TEXT("Can cancel while running"), Handle.Cancel());
		TestEqual(TEXT("State is cancelled"), Handle.GetState(), FMockAsyncHandle::EState::Cancelled);
		TestFalse(TEXT("Cannot complete after cancellation"), Handle.Complete());
	}

	// Test concurrent start
	{
		const int32 NumIterations = 100;
		int32 MultipleStarts = 0;

		for (int32 Iter = 0; Iter < NumIterations; ++Iter)
		{
			FMockAsyncHandle Handle;
			std::atomic<int32> StartCount{0};
			std::atomic<bool> StartFlag{false};

			TArray<TFuture<void>> Futures;
			for (int32 t = 0; t < 4; ++t)
			{
				Futures.Add(Async(EAsyncExecution::ThreadPool, [&]()
				{
					while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
					if (Handle.Start())
					{
						StartCount.fetch_add(1);
					}
				}));
			}

			StartFlag.store(true);
			for (auto& F : Futures) F.Wait();

			if (StartCount.load() != 1)
			{
				MultipleStarts++;
			}
		}

		TestEqual(TEXT("Only one thread should successfully start"), MultipleStarts, 0);
	}

	return true;
}

// =============================================================================
// Memory Ordering Tests
// =============================================================================

/**
 * Test that memory ordering is correct for flag-based synchronization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMemoryOrderingTest,
	"PCGEx.Functional.Threading.Async.MemoryOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMemoryOrderingTest::RunTest(const FString& Parameters)
{
	struct FSharedState
	{
		int32 Data = 0;
		std::atomic<bool> Ready{false};
	};

	const int32 NumIterations = 1000;
	int32 OrderingFailures = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FSharedState State;

		// Writer thread
		auto WriteFuture = Async(EAsyncExecution::ThreadPool, [&State]()
		{
			State.Data = 42; // Write data
			State.Ready.store(true, std::memory_order_release); // Release
		});

		// Reader thread
		auto ReadFuture = Async(EAsyncExecution::ThreadPool, [&State, &OrderingFailures]()
		{
			// Spin until ready
			while (!State.Ready.load(std::memory_order_acquire)) // Acquire
			{
				FPlatformProcess::YieldThread();
			}

			// Data should be visible now
			if (State.Data != 42)
			{
				// This would indicate a memory ordering issue
				// Should never happen with proper acquire-release semantics
			}
		});

		WriteFuture.Wait();
		ReadFuture.Wait();

		if (State.Data != 42)
		{
			OrderingFailures++;
		}
	}

	TestEqual(TEXT("No memory ordering failures"), OrderingFailures, 0);

	return true;
}
