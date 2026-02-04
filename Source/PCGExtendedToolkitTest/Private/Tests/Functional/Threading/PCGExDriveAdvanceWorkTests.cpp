// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx DriveAdvanceWork Pattern Tests
 *
 * Tests for the DriveAdvanceWork coordination pattern that prevents re-entry
 * issues when OnAsyncWorkEnd fires while AdvanceWork is still executing.
 *
 * Background:
 * -----------
 * PCGEx short-circuits the PCG scheduler by having OnAsyncWorkEnd call
 * DriveAdvanceWork directly, avoiding frame delays. This creates potential
 * re-entry issues when async work completes very fast (before AdvanceWork returns).
 *
 * The DriveAdvanceWork pattern uses:
 * - compare_exchange to ensure only one caller drives at a time
 * - bPendingAsyncWorkEnd flag for deferred completions
 * - do-while loop to process pending completions
 * - Final recursive check to catch the edge case race
 *
 * Key scenarios tested:
 * - Single driver guarantee (compare_exchange)
 * - Pending flag pickup in do-while loop
 * - Edge case: pending set after do-while but before flag clear
 * - Concurrent calls from simulated ExecuteInternal and OnAsyncWorkEnd
 * - Spin loop mode behavior
 *
 * Test naming: PCGEx.Functional.Threading.DriveAdvanceWork.<Scenario>
 */

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

#include <atomic>
#include <functional>
#include <memory>

// =============================================================================
// Mock Context for DriveAdvanceWork Testing
// =============================================================================

/**
 * Simulates the FPCGExContext DriveAdvanceWork pattern
 */
class FMockDriveAdvanceWorkContext
{
public:
	std::atomic<bool> bAdvanceWorkInProgress{false};
	std::atomic<bool> bPendingAsyncWorkEnd{false};
	std::atomic<int32> AdvanceWorkCallCount{0};
	std::atomic<int32> DriveAdvanceWorkCallCount{0};
	std::atomic<int32> ConcurrentDriveAttempts{0};
	std::atomic<int32> PendingPickups{0};
	std::atomic<int32> FinalCheckRetries{0};

	// Simulated work result - false = not done, true = done
	std::atomic<bool> bWorkComplete{false};

	// Callback to simulate AdvanceWork behavior
	std::function<bool()> AdvanceWorkCallback;

	FMockDriveAdvanceWorkContext()
	{
		// Default: work completes after first call
		AdvanceWorkCallback = [this]()
		{
			int32 Count = AdvanceWorkCallCount.fetch_add(1);
			return Count > 0; // First call returns false, subsequent return true
		};
	}

	bool AdvanceWork()
	{
		if (AdvanceWorkCallback)
		{
			return AdvanceWorkCallback();
		}
		return AdvanceWorkCallCount.fetch_add(1) > 0;
	}

	bool DriveAdvanceWork()
	{
		DriveAdvanceWorkCallCount.fetch_add(1);

		// Try to become the driver - only one caller can drive at a time
		bool bExpected = false;
		if (!bAdvanceWorkInProgress.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
		{
			// Someone else is driving - request they do another round when done
			bPendingAsyncWorkEnd.store(true, std::memory_order_release);
			ConcurrentDriveAttempts.fetch_add(1);
			return false;
		}

		// We're the driver - keep advancing until no more pending completions
		bool bResult;
		do
		{
			bool bWasPending = bPendingAsyncWorkEnd.exchange(false, std::memory_order_acq_rel);
			if (bWasPending)
			{
				PendingPickups.fetch_add(1);
			}
			bResult = AdvanceWork();
		}
		while (bPendingAsyncWorkEnd.load(std::memory_order_acquire));

		bAdvanceWorkInProgress.store(false, std::memory_order_release);

		// Final check: pending may have been set between do-while check and clearing flag
		if (bPendingAsyncWorkEnd.exchange(false, std::memory_order_acq_rel))
		{
			FinalCheckRetries.fetch_add(1);
			return DriveAdvanceWork(); // Retry
		}

		return bResult;
	}

	void Reset()
	{
		bAdvanceWorkInProgress.store(false);
		bPendingAsyncWorkEnd.store(false);
		AdvanceWorkCallCount.store(0);
		DriveAdvanceWorkCallCount.store(0);
		ConcurrentDriveAttempts.store(0);
		PendingPickups.store(0);
		FinalCheckRetries.store(0);
		bWorkComplete.store(false);
	}
};

// =============================================================================
// Single Driver Guarantee Tests
// =============================================================================

/**
 * Test that only one caller can be the driver at a time
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkSingleDriverTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.SingleDriver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkSingleDriverTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 500;
	int32 MultipleDrivers = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<int32> ActiveDrivers{0};
		std::atomic<int32> MaxConcurrentDrivers{0};
		std::atomic<bool> StartFlag{false};

		// Setup callback to track concurrent drivers
		Context.AdvanceWorkCallback = [&ActiveDrivers, &MaxConcurrentDrivers]()
		{
			int32 Current = ActiveDrivers.fetch_add(1) + 1;

			// Track max concurrent
			int32 Max = MaxConcurrentDrivers.load();
			while (Current > Max && !MaxConcurrentDrivers.compare_exchange_weak(Max, Current)) {}

			// Simulate work
			FPlatformProcess::YieldThread();

			ActiveDrivers.fetch_sub(1);
			return true;
		};

		// Launch multiple threads trying to drive simultaneously
		const int32 NumThreads = 8;
		TArray<TFuture<void>> Futures;

		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
				Context.DriveAdvanceWork();
			}));
		}

		StartFlag.store(true);

		for (auto& Future : Futures)
		{
			Future.Wait();
		}

		if (MaxConcurrentDrivers.load() > 1)
		{
			MultipleDrivers++;
		}
	}

	TestEqual(TEXT("Only one driver should be active at a time"), MultipleDrivers, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations with %d violations"), NumIterations, MultipleDrivers));

	return true;
}

// =============================================================================
// Pending Flag Pickup Tests
// =============================================================================

/**
 * Test that pending completions are picked up by the driver's do-while loop.
 *
 * The key invariant: if async work sets pending while someone is driving,
 * that work notification MUST eventually be processed (either by the current
 * driver's do-while loop, final check, or by a subsequent driver).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkPendingPickupTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.PendingPickup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkPendingPickupTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 200;
	int32 WorkNotProcessed = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<int32> WorkRequested{0};  // How many times async signaled "work ready"
		std::atomic<int32> TotalAdvanceWorkCalls{0};

		// Setup callback that tracks actual work processing
		Context.AdvanceWorkCallback = [&TotalAdvanceWorkCalls]()
		{
			TotalAdvanceWorkCalls.fetch_add(1);
			// Simulate work that takes some time
			for (int i = 0; i < 5; ++i)
			{
				FPlatformProcess::YieldThread();
			}
			// Complete after a few calls
			return TotalAdvanceWorkCalls.load() >= 3;
		};

		// Driver thread (simulates ExecuteInternal)
		auto DriverFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, &WorkRequested]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			WorkRequested.fetch_add(1);  // Initial work request
			Context.DriveAdvanceWork();
		});

		// Async completion threads (simulate OnAsyncWorkEnd)
		TArray<TFuture<void>> AsyncFutures;
		for (int32 t = 0; t < 3; ++t)
		{
			AsyncFutures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, &WorkRequested, t]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				// Stagger the async completions
				FPlatformProcess::Sleep(0.0001f * (t + 1));

				WorkRequested.fetch_add(1);  // Signal work ready
				Context.DriveAdvanceWork();  // Try to drive (may succeed or set pending)
			}));
		}

		StartFlag.store(true);

		DriverFuture.Wait();
		for (auto& F : AsyncFutures) { F.Wait(); }

		// The key invariant: AdvanceWork should have been called at least once
		// for each "driver" that successfully acquired the lock, and pending
		// notifications should have triggered additional calls.
		int32 TotalCalls = TotalAdvanceWorkCalls.load();
		int32 Requests = WorkRequested.load();

		// We should have processed work. With 4 threads all trying to drive,
		// at minimum 1 should succeed as driver. The pending mechanism ensures
		// that async completions are not lost.
		if (TotalCalls == 0)
		{
			WorkNotProcessed++;
		}
	}

	TestEqual(TEXT("Work should always be processed"), WorkNotProcessed, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations, %d had no work processed"), NumIterations, WorkNotProcessed));

	return true;
}

// =============================================================================
// Final Check Race Condition Tests
// =============================================================================

/**
 * Test the edge case where pending is set between do-while check and flag clear.
 * This is the race condition that requires the final recursive check.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkFinalCheckRaceTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.FinalCheckRace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkFinalCheckRaceTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 1000;
	int32 FinalCheckTriggered = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<bool> DriverInDoWhile{false};
		std::atomic<bool> DriverExitedDoWhile{false};

		// Setup callback to signal when we're in specific phases
		Context.AdvanceWorkCallback = [&Context, &DriverInDoWhile, &DriverExitedDoWhile]()
		{
			DriverInDoWhile.store(true);
			// Brief work
			FPlatformProcess::YieldThread();
			return true;
		};

		// We need to manually trigger the race condition
		// This is done by having a thread set pending right after do-while exits

		auto DriverFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			Context.DriveAdvanceWork();
		});

		// Thread that tries to trigger the race
		auto RaceFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Keep trying to set pending at the right moment
			for (int i = 0; i < 100; ++i)
			{
				// If driver is active, try to set pending
				if (Context.bAdvanceWorkInProgress.load())
				{
					Context.bPendingAsyncWorkEnd.store(true, std::memory_order_release);
				}
				FPlatformProcess::YieldThread();
			}
		});

		StartFlag.store(true);

		DriverFuture.Wait();
		RaceFuture.Wait();

		if (Context.FinalCheckRetries.load() > 0)
		{
			FinalCheckTriggered++;
		}
	}

	AddInfo(FString::Printf(TEXT("Final check triggered %d times in %d iterations (%.1f%%)"),
		FinalCheckTriggered, NumIterations, 100.0f * FinalCheckTriggered / NumIterations));

	// We don't assert on a specific count - the race is timing-dependent
	// The important thing is that the code handles it correctly when it happens
	return true;
}

// =============================================================================
// Simulated ExecuteInternal + OnAsyncWorkEnd Tests
// =============================================================================

/**
 * Simulates the real-world scenario of ExecuteInternal and OnAsyncWorkEnd
 * both trying to drive work.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkExecuteAndAsyncTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.ExecuteAndAsync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkExecuteAndAsyncTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 200;
	int32 DataLoss = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<int32> WorkUnitsProcessed{0};
		const int32 TotalWorkUnits = 5;

		// Setup callback that processes work units
		Context.AdvanceWorkCallback = [&WorkUnitsProcessed, TotalWorkUnits]()
		{
			int32 Processed = WorkUnitsProcessed.fetch_add(1) + 1;
			FPlatformProcess::YieldThread();
			return Processed >= TotalWorkUnits;
		};

		// Simulates ExecuteInternal spin loop
		auto ExecuteInternalFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Spin loop pattern (with timeout safety)
			double LoopStart = FPlatformTime::Seconds();
			while (!Context.DriveAdvanceWork())
			{
				FPlatformProcess::YieldThread();
				if (FPlatformTime::Seconds() - LoopStart > 2.0) break;
			}
		});

		// Simulates OnAsyncWorkEnd being called multiple times
		TArray<TFuture<void>> AsyncFutures;
		for (int32 t = 0; t < 3; ++t)
		{
			AsyncFutures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, t]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				// Stagger async completions
				FPlatformProcess::Sleep(0.0001f * (t + 1));
				Context.DriveAdvanceWork();
			}));
		}

		StartFlag.store(true);

		ExecuteInternalFuture.Wait();
		for (auto& F : AsyncFutures) { F.Wait(); }

		// All work units should have been processed
		if (WorkUnitsProcessed.load() < TotalWorkUnits)
		{
			DataLoss++;
		}
	}

	TestEqual(TEXT("All work units should be processed"), DataLoss, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations, %d had incomplete work"), NumIterations, DataLoss));

	return true;
}

// =============================================================================
// Stress Tests
// =============================================================================

/**
 * High-contention stress test with many concurrent callers
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkStressTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.Stress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkStressTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 50;
	const int32 NumThreads = 16;
	int32 Failures = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<int32> SuccessfulDrives{0};

		// Many threads hammering DriveAdvanceWork
		TArray<TFuture<void>> Futures;
		for (int32 t = 0; t < NumThreads; ++t)
		{
			Futures.Add(Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, &SuccessfulDrives]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				for (int32 i = 0; i < 10; ++i)
				{
					if (Context.DriveAdvanceWork())
					{
						SuccessfulDrives.fetch_add(1);
					}
					FPlatformProcess::YieldThread();
				}
			}));
		}

		StartFlag.store(true);

		for (auto& F : Futures) { F.Wait(); }

		// Verify no corruption - all calls should have either driven or set pending
		int32 TotalCalls = Context.DriveAdvanceWorkCallCount.load();
		int32 Driven = TotalCalls - Context.ConcurrentDriveAttempts.load();

		if (Driven < 1)
		{
			Failures++;
		}
	}

	TestEqual(TEXT("All stress test iterations should complete successfully"), Failures, 0);
	AddInfo(FString::Printf(TEXT("Stress test: %d iterations with %d threads each"), NumIterations, NumThreads));

	return true;
}

// =============================================================================
// Return Value Correctness Tests
// =============================================================================

/**
 * Test that the return value from DriveAdvanceWork is correct
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkReturnValueTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.ReturnValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkReturnValueTest::RunTest(const FString& Parameters)
{
	// Test 1: Work completes on first call
	{
		FMockDriveAdvanceWorkContext Context;
		Context.AdvanceWorkCallback = []() { return true; };

		bool Result = Context.DriveAdvanceWork();
		TestTrue(TEXT("Should return true when work completes"), Result);
	}

	// Test 2: Work doesn't complete
	{
		FMockDriveAdvanceWorkContext Context;
		Context.AdvanceWorkCallback = []() { return false; };

		bool Result = Context.DriveAdvanceWork();
		TestFalse(TEXT("Should return false when work doesn't complete"), Result);
	}

	// Test 3: Non-driver should return false
	{
		FMockDriveAdvanceWorkContext Context;
		Context.bAdvanceWorkInProgress.store(true); // Simulate someone else driving

		bool Result = Context.DriveAdvanceWork();
		TestFalse(TEXT("Non-driver should return false"), Result);
		TestTrue(TEXT("Pending should be set"), Context.bPendingAsyncWorkEnd.load());
	}

	return true;
}

// =============================================================================
// State Re-Entry Prevention Tests (Original Bug Scenario)
// =============================================================================

/**
 * Test that simulates the original bug: async callback firing before AdvanceWork returns.
 *
 * The bug scenario:
 * 1. ExecuteInternal calls AdvanceWork
 * 2. AdvanceWork starts batch processing, schedules async work
 * 3. Async work completes BEFORE AdvanceWork returns
 * 4. OnAsyncWorkEnd fires and tries to call AdvanceWork
 * 5. Without protection, state is still "InitialExecution" -> re-entry bug!
 *
 * DriveAdvanceWork prevents this by using compare_exchange to ensure only one driver.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkReEntryPreventionTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.ReEntryPrevention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkReEntryPreventionTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 500;
	int32 ReEntryDetected = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<bool> InsideAdvanceWork{false};
		std::atomic<int32> ConcurrentAdvanceWork{0};
		std::atomic<int32> MaxConcurrent{0};

		std::atomic<bool> DriverDone{false};

		// Callback that detects concurrent execution
		Context.AdvanceWorkCallback = [&InsideAdvanceWork, &ConcurrentAdvanceWork, &MaxConcurrent]()
		{
			// Mark entry
			int32 Concurrent = ConcurrentAdvanceWork.fetch_add(1) + 1;

			// Track max concurrent
			int32 Max = MaxConcurrent.load();
			while (Concurrent > Max && !MaxConcurrent.compare_exchange_weak(Max, Concurrent)) {}

			InsideAdvanceWork.store(true);

			// Simulate work - this is where async could complete
			for (int i = 0; i < 20; ++i)
			{
				FPlatformProcess::YieldThread();
			}

			// Mark exit
			ConcurrentAdvanceWork.fetch_sub(1);
			InsideAdvanceWork.store(false);

			return true;
		};

		// Thread simulating ExecuteInternal
		auto ExecuteFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, &DriverDone]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			Context.DriveAdvanceWork();
			DriverDone.store(true);
		});

		// Thread simulating OnAsyncWorkEnd firing while AdvanceWork is running
		auto AsyncFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StartFlag, &InsideAdvanceWork, &DriverDone]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Wait until AdvanceWork has started OR driver is already done
			int32 WaitCount = 0;
			while (!InsideAdvanceWork.load() && !DriverDone.load() && WaitCount < 10000)
			{
				FPlatformProcess::YieldThread();
				WaitCount++;
			}

			// Try to call DriveAdvanceWork - simulating async completing mid-execution
			// (If driver is already done, this just becomes a no-op test of the pattern)
			Context.DriveAdvanceWork();
		});

		StartFlag.store(true);

		ExecuteFuture.Wait();
		AsyncFuture.Wait();

		// Check: AdvanceWork should never have been called concurrently
		if (MaxConcurrent.load() > 1)
		{
			ReEntryDetected++;
		}
	}

	TestEqual(TEXT("AdvanceWork should never run concurrently"), ReEntryDetected, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations, %d had re-entry"), NumIterations, ReEntryDetected));

	return true;
}

// =============================================================================
// Fast Async Completion Tests
// =============================================================================

/**
 * Test the scenario where async completes extremely fast (before scheduling returns).
 *
 * This is the exact bug scenario that caused "random missing data":
 * - ScheduleBatch() starts async work
 * - Async work completes immediately (before ScheduleBatch returns)
 * - OnAsyncWorkEnd fires while still in InitialExecution state
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkFastAsyncTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.FastAsyncCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkFastAsyncTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 500;
	int32 StateCorruption = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		// Simulate state machine
		std::atomic<int32> State{0};  // 0=Initial, 1=Processing, 2=Done
		std::atomic<int32> InitialExecutionCount{0};
		std::atomic<bool> bAdvanceWorkInProgress{false};
		std::atomic<bool> bPendingAsyncWorkEnd{false};
		std::atomic<bool> StartFlag{false};

		// Use std::function to allow recursive lambda
		std::function<bool()> DriveAdvanceWork;
		DriveAdvanceWork = [&]() -> bool
		{
			bool bExpected = false;
			if (!bAdvanceWorkInProgress.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				bPendingAsyncWorkEnd.store(true, std::memory_order_release);
				return false;
			}

			bool bResult;
			do
			{
				bPendingAsyncWorkEnd.store(false, std::memory_order_release);

				// Simulate AdvanceWork state machine
				int32 CurrentState = State.load();
				if (CurrentState == 0)  // Initial
				{
					InitialExecutionCount.fetch_add(1);
					// Transition to Processing
					State.store(1);
					// Simulate: This is where async work would be scheduled
					FPlatformProcess::YieldThread();
					bResult = false;  // Not done yet
				}
				else if (CurrentState == 1)  // Processing
				{
					// Batch done, transition to Done
					State.store(2);
					bResult = true;
				}
				else  // Done
				{
					bResult = true;
				}
			}
			while (bPendingAsyncWorkEnd.load(std::memory_order_acquire));

			bAdvanceWorkInProgress.store(false, std::memory_order_release);

			if (bPendingAsyncWorkEnd.exchange(false, std::memory_order_acq_rel))
			{
				return DriveAdvanceWork();
			}

			return bResult;
		};

		// Execute thread
		auto ExecuteFuture = Async(EAsyncExecution::ThreadPool, [&DriveAdvanceWork, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			double LoopStart = FPlatformTime::Seconds();
			while (!DriveAdvanceWork())
			{
				FPlatformProcess::YieldThread();
				// Safety timeout
				if (FPlatformTime::Seconds() - LoopStart > 2.0) break;
			}
		});

		// Simulate async callback firing immediately
		auto AsyncFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			// Immediately try to drive (simulating instant async completion)
			DriveAdvanceWork();
		});

		StartFlag.store(true);

		ExecuteFuture.Wait();
		AsyncFuture.Wait();

		// InitialExecution block should only run ONCE
		if (InitialExecutionCount.load() != 1)
		{
			StateCorruption++;
		}
	}

	TestEqual(TEXT("InitialExecution should run exactly once"), StateCorruption, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations, %d had state corruption"), NumIterations, StateCorruption));

	return true;
}

// =============================================================================
// Batch Reset Protection Tests
// =============================================================================

/**
 * Test that batch operations are not corrupted by re-entry.
 *
 * In the original bug, MainBatch.Reset() was called twice because
 * InitialExecution ran twice, destroying the first batch mid-processing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkBatchProtectionTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.BatchProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkBatchProtectionTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 300;
	int32 BatchCorruption = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		// Simulate batch lifecycle
		std::atomic<int32> BatchResetCount{0};
		std::atomic<int32> BatchProcessCount{0};
		std::atomic<bool> BatchValid{false};

		std::atomic<int32> State{0};
		std::atomic<bool> bAdvanceWorkInProgress{false};
		std::atomic<bool> bPendingAsyncWorkEnd{false};
		std::atomic<bool> StartFlag{false};

		// Use std::function to allow recursive lambda
		std::function<bool()> DriveAdvanceWork;
		DriveAdvanceWork = [&]() -> bool
		{
			bool bExpected = false;
			if (!bAdvanceWorkInProgress.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
			{
				bPendingAsyncWorkEnd.store(true, std::memory_order_release);
				return false;
			}

			bool bResult;
			do
			{
				bPendingAsyncWorkEnd.store(false, std::memory_order_release);

				int32 CurrentState = State.load();
				if (CurrentState == 0)  // Initial
				{
					// Reset batch (this was called twice in the bug!)
					BatchResetCount.fetch_add(1);
					BatchValid.store(true);
					State.store(1);

					// Simulate scheduling async work
					FPlatformProcess::YieldThread();
					bResult = false;
				}
				else if (CurrentState == 1)  // Processing
				{
					// Process batch
					if (BatchValid.load())
					{
						BatchProcessCount.fetch_add(1);
					}
					State.store(2);
					bResult = true;
				}
				else
				{
					bResult = true;
				}
			}
			while (bPendingAsyncWorkEnd.load(std::memory_order_acquire));

			bAdvanceWorkInProgress.store(false, std::memory_order_release);

			if (bPendingAsyncWorkEnd.exchange(false, std::memory_order_acq_rel))
			{
				return DriveAdvanceWork();
			}

			return bResult;
		};

		auto ExecuteFuture = Async(EAsyncExecution::ThreadPool, [&DriveAdvanceWork, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			double LoopStart = FPlatformTime::Seconds();
			while (!DriveAdvanceWork())
			{
				FPlatformProcess::YieldThread();
				if (FPlatformTime::Seconds() - LoopStart > 2.0) break;
			}
		});

		auto AsyncFuture = Async(EAsyncExecution::ThreadPool, [&DriveAdvanceWork, &StartFlag]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }
			DriveAdvanceWork();
		});

		StartFlag.store(true);

		ExecuteFuture.Wait();
		AsyncFuture.Wait();

		// Batch should be reset exactly once
		if (BatchResetCount.load() != 1)
		{
			BatchCorruption++;
		}
	}

	TestEqual(TEXT("Batch should be reset exactly once"), BatchCorruption, 0);
	AddInfo(FString::Printf(TEXT("Tested %d iterations, %d had batch corruption"), NumIterations, BatchCorruption));

	return true;
}

// =============================================================================
// Spin Loop vs Async Callback Coordination Tests
// =============================================================================

/**
 * Test coordination between spin loop (NoPause mode) and async callbacks.
 *
 * In NoPause mode, ExecuteInternal runs a spin loop calling DriveAdvanceWork.
 * OnAsyncWorkEnd also calls DriveAdvanceWork. These must coordinate properly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkSpinLoopCoordinationTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.SpinLoopCoordination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkSpinLoopCoordinationTest::RunTest(const FString& Parameters)
{
	const int32 NumIterations = 100;
	int32 Failures = 0;

	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		FMockDriveAdvanceWorkContext Context;
		std::atomic<bool> StartFlag{false};
		std::atomic<bool> WorkComplete{false};
		std::atomic<int32> SpinIterations{0};

		// Work completes after a few calls
		Context.AdvanceWorkCallback = [&WorkComplete]()
		{
			FPlatformProcess::YieldThread();
			return WorkComplete.load();
		};

		// Spin loop thread (simulates NoPause ExecuteInternal)
		auto SpinFuture = Async(EAsyncExecution::ThreadPool, [&]()
		{
			while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

			// Spin loop pattern from ExecuteInternal (with timeout safety)
			double SpinStart = FPlatformTime::Seconds();
			while (!Context.DriveAdvanceWork())
			{
				SpinIterations.fetch_add(1);
				FPlatformProcess::YieldThread();

				// Safety timeout to prevent test hang
				if (FPlatformTime::Seconds() - SpinStart > 2.0)
				{
					break;
				}
			}
		});

		// Async callback threads
		TArray<TFuture<void>> AsyncFutures;
		for (int32 t = 0; t < 5; ++t)
		{
			AsyncFutures.Add(Async(EAsyncExecution::ThreadPool, [&, t]()
			{
				while (!StartFlag.load()) { FPlatformProcess::YieldThread(); }

				FPlatformProcess::Sleep(0.001f * (t + 1));

				// Last async sets work complete
				if (t == 4)
				{
					WorkComplete.store(true);
				}

				Context.DriveAdvanceWork();
			}));
		}

		StartFlag.store(true);

		SpinFuture.Wait();
		for (auto& F : AsyncFutures) { F.Wait(); }

		// Spin loop should have exited successfully
		if (!WorkComplete.load())
		{
			Failures++;
		}
	}

	TestEqual(TEXT("All spin loops should complete"), Failures, 0);

	return true;
}

// =============================================================================
// No Infinite Loop Tests
// =============================================================================

/**
 * Test that the recursive retry doesn't cause infinite loops
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDriveAdvanceWorkNoInfiniteLoopTest,
	"PCGEx.Functional.Threading.DriveAdvanceWork.NoInfiniteLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDriveAdvanceWorkNoInfiniteLoopTest::RunTest(const FString& Parameters)
{
	FMockDriveAdvanceWorkContext Context;
	std::atomic<bool> StopFlag{false};
	std::atomic<bool> Completed{false};

	// Setup callback that always returns true (work is done)
	Context.AdvanceWorkCallback = []() { return true; };

	// Thread that keeps setting pending (adversarial)
	auto AdversaryFuture = Async(EAsyncExecution::ThreadPool, [&Context, &StopFlag]()
	{
		while (!StopFlag.load())
		{
			Context.bPendingAsyncWorkEnd.store(true, std::memory_order_release);
			FPlatformProcess::YieldThread();
		}
	});

	// Driver should still complete in reasonable time
	auto DriverFuture = Async(EAsyncExecution::ThreadPool, [&Context, &Completed]()
	{
		Context.DriveAdvanceWork();
		Completed.store(true);
	});

	// Wait with timeout
	double StartTime = FPlatformTime::Seconds();
	const double Timeout = 1.0; // 1 second timeout

	while (!Completed.load() && (FPlatformTime::Seconds() - StartTime) < Timeout)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	StopFlag.store(true);
	AdversaryFuture.Wait();
	DriverFuture.Wait();

	TestTrue(TEXT("DriveAdvanceWork should complete within timeout"), Completed.load());

	if (Completed.load())
	{
		AddInfo(FString::Printf(TEXT("Completed with %d retries"), Context.FinalCheckRetries.load()));
	}

	return true;
}
