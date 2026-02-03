// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Thread Safety Functional Tests
 *
 * Tests parallel processing patterns used in PCGEx to ensure
 * thread-safe buffer writes and data access patterns.
 *
 * Key patterns tested:
 * - Parallel writes to unique indices (safe)
 * - Parallel reads from shared data (safe)
 * - Pre-allocated buffer patterns
 *
 * These tests verify the correctness of parallel processing
 * without actually using PCGEx threading (which requires context).
 *
 * Test naming: PCGEx.Functional.Threading.<Pattern>
 */

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"

// =============================================================================
// Parallel Buffer Write Pattern Tests
// =============================================================================

/**
 * Test parallel writes to unique indices (the PCGEx pattern)
 *
 * This pattern is SAFE because each thread writes to a unique index,
 * preventing race conditions without locks.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingUniqueIndexWriteTest,
	"PCGEx.Functional.Threading.UniqueIndexWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingUniqueIndexWriteTest::RunTest(const FString& Parameters)
{
	const int32 NumElements = 10000;

	// Pre-allocate buffer (critical for thread safety)
	TArray<int32> Buffer;
	Buffer.SetNumUninitialized(NumElements);

	// Initialize to known invalid value
	for (int32& Value : Buffer)
	{
		Value = -1;
	}

	// Parallel write - each thread writes to unique index
	// This is the safe pattern used in PCGEX_SCOPE_LOOP
	ParallelFor(NumElements, [&Buffer](int32 Index)
	{
		// SAFE: Each iteration writes to unique Buffer[Index]
		Buffer[Index] = Index * 2;
	});

	// Verify all writes succeeded
	bool AllCorrect = true;
	for (int32 i = 0; i < NumElements; ++i)
	{
		if (Buffer[i] != i * 2)
		{
			AllCorrect = false;
			AddError(FString::Printf(TEXT("Buffer[%d] = %d, expected %d"), i, Buffer[i], i * 2));
			break;
		}
	}

	TestTrue(TEXT("All parallel writes succeeded"), AllCorrect);

	return true;
}

/**
 * Test parallel writes with computed values (more complex operations)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingComputedWriteTest,
	"PCGEx.Functional.Threading.ComputedWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingComputedWriteTest::RunTest(const FString& Parameters)
{
	const int32 NumElements = 1000;

	// Multiple buffers for different output types
	TArray<FVector> Positions;
	TArray<float> Distances;
	TArray<bool> Flags;

	Positions.SetNumUninitialized(NumElements);
	Distances.SetNumUninitialized(NumElements);
	Flags.SetNumUninitialized(NumElements);

	// Shared read-only reference point
	const FVector ReferencePoint(500, 500, 0);

	// Parallel compute and write
	ParallelFor(NumElements, [&](int32 Index)
	{
		// Compute position (each writes to unique index)
		float X = static_cast<float>(Index % 100) * 10.0f;
		float Y = static_cast<float>(Index / 100) * 10.0f;
		Positions[Index] = FVector(X, Y, 0);

		// Compute distance (read from our own position, write to unique index)
		Distances[Index] = FVector::Dist(Positions[Index], ReferencePoint);

		// Compute flag (write to unique index)
		Flags[Index] = Distances[Index] < 300.0f;
	});

	// Verify results
	int32 ErrorCount = 0;
	for (int32 i = 0; i < NumElements && ErrorCount < 5; ++i)
	{
		float ExpectedX = static_cast<float>(i % 100) * 10.0f;
		float ExpectedY = static_cast<float>(i / 100) * 10.0f;
		FVector ExpectedPos(ExpectedX, ExpectedY, 0);

		if (!Positions[i].Equals(ExpectedPos, KINDA_SMALL_NUMBER))
		{
			AddError(FString::Printf(TEXT("Position[%d] incorrect"), i));
			ErrorCount++;
		}

		float ExpectedDist = FVector::Dist(ExpectedPos, ReferencePoint);
		if (!FMath::IsNearlyEqual(Distances[i], ExpectedDist, 0.01f))
		{
			AddError(FString::Printf(TEXT("Distance[%d] incorrect"), i));
			ErrorCount++;
		}

		bool ExpectedFlag = ExpectedDist < 300.0f;
		if (Flags[i] != ExpectedFlag)
		{
			AddError(FString::Printf(TEXT("Flag[%d] incorrect"), i));
			ErrorCount++;
		}
	}

	TestEqual(TEXT("No errors in parallel compute"), ErrorCount, 0);

	return true;
}

// =============================================================================
// Parallel Read Pattern Tests
// =============================================================================

/**
 * Test parallel reads from shared immutable data (safe pattern)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingSharedReadTest,
	"PCGEx.Functional.Threading.SharedRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingSharedReadTest::RunTest(const FString& Parameters)
{
	const int32 NumElements = 5000;

	// Shared immutable source data
	TArray<FVector> SourcePositions;
	SourcePositions.Reserve(100);
	for (int32 i = 0; i < 100; ++i)
	{
		SourcePositions.Add(FVector(i * 10.0f, 0, 0));
	}

	// Output buffer
	TArray<FVector> Results;
	Results.SetNumUninitialized(NumElements);

	// Parallel read from shared source, write to unique output
	ParallelFor(NumElements, [&](int32 Index)
	{
		// SAFE: Read from shared immutable data
		int32 SourceIndex = Index % 100;
		FVector SourcePos = SourcePositions[SourceIndex];

		// Compute something using the read data
		// SAFE: Write to unique output index
		Results[Index] = SourcePos + FVector(0, Index * 0.1f, 0);
	});

	// Verify all reads/writes succeeded
	bool AllCorrect = true;
	for (int32 i = 0; i < NumElements && AllCorrect; ++i)
	{
		int32 SourceIndex = i % 100;
		FVector Expected = SourcePositions[SourceIndex] + FVector(0, i * 0.1f, 0);

		if (!Results[i].Equals(Expected, KINDA_SMALL_NUMBER))
		{
			AllCorrect = false;
			AddError(FString::Printf(TEXT("Result[%d] incorrect"), i));
		}
	}

	TestTrue(TEXT("All parallel reads succeeded"), AllCorrect);

	return true;
}

// =============================================================================
// Buffer Pre-allocation Pattern Tests
// =============================================================================

/**
 * Test that pre-allocation is necessary for safe parallel writes
 * This demonstrates why SetNumUninitialized is used before parallel loops
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingPreallocationTest,
	"PCGEx.Functional.Threading.Preallocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingPreallocationTest::RunTest(const FString& Parameters)
{
	const int32 NumElements = 1000;

	// CORRECT: Pre-allocate with SetNumUninitialized
	TArray<int32> CorrectBuffer;
	CorrectBuffer.SetNumUninitialized(NumElements);

	TestEqual(TEXT("Pre-allocated buffer has correct size"),
	          CorrectBuffer.Num(), NumElements);

	// CORRECT: Pre-allocate with Init
	TArray<int32> InitializedBuffer;
	InitializedBuffer.Init(0, NumElements);

	TestEqual(TEXT("Initialized buffer has correct size"),
	          InitializedBuffer.Num(), NumElements);

	// Verify Init sets values
	bool AllZero = true;
	for (int32 Value : InitializedBuffer)
	{
		if (Value != 0)
		{
			AllZero = false;
			break;
		}
	}
	TestTrue(TEXT("Init buffer values are zero"), AllZero);

	// CORRECT: Reserve + SetNum pattern
	TArray<float> ReservedBuffer;
	ReservedBuffer.Reserve(NumElements);
	ReservedBuffer.SetNum(NumElements, EAllowShrinking::No); // false = don't initialize

	TestEqual(TEXT("Reserve+SetNum buffer has correct size"),
	          ReservedBuffer.Num(), NumElements);

	// Now parallel write should be safe
	ParallelFor(NumElements, [&](int32 Index)
	{
		CorrectBuffer[Index] = Index;
		InitializedBuffer[Index] = Index * 2;
		ReservedBuffer[Index] = static_cast<float>(Index) * 0.5f;
	});

	// Verify writes
	TestEqual(TEXT("First buffer write correct"), CorrectBuffer[500], 500);
	TestEqual(TEXT("Second buffer write correct"), InitializedBuffer[500], 1000);
	TestTrue(TEXT("Third buffer write correct"),
	         FMath::IsNearlyEqual(ReservedBuffer[500], 250.0f, 0.001f));

	return true;
}

// =============================================================================
// Index Mapping Pattern Tests
// =============================================================================

/**
 * Test parallel index remapping (common in filtering operations)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingIndexMappingTest,
	"PCGEx.Functional.Threading.IndexMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingIndexMappingTest::RunTest(const FString& Parameters)
{
	const int32 SourceCount = 1000;

	// Source data
	TArray<int32> SourceValues;
	SourceValues.SetNumUninitialized(SourceCount);
	for (int32 i = 0; i < SourceCount; ++i)
	{
		SourceValues[i] = i * 3;
	}

	// Index mapping: new -> old (simulating filtered subset)
	TArray<int32> IndexMap;
	IndexMap.Reserve(SourceCount / 2);
	for (int32 i = 0; i < SourceCount; i += 2) // Keep even indices
	{
		IndexMap.Add(i);
	}

	const int32 OutputCount = IndexMap.Num();
	TestEqual(TEXT("Index map has 500 entries"), OutputCount, 500);

	// Output buffer
	TArray<int32> OutputValues;
	OutputValues.SetNumUninitialized(OutputCount);

	// Parallel gather using index map
	ParallelFor(OutputCount, [&](int32 NewIndex)
	{
		// SAFE: Read from shared IndexMap and SourceValues
		int32 OldIndex = IndexMap[NewIndex];

		// SAFE: Write to unique output index
		OutputValues[NewIndex] = SourceValues[OldIndex];
	});

	// Verify mapping worked
	bool AllCorrect = true;
	for (int32 NewIdx = 0; NewIdx < OutputCount && AllCorrect; ++NewIdx)
	{
		int32 OldIdx = IndexMap[NewIdx];
		int32 Expected = OldIdx * 3;

		if (OutputValues[NewIdx] != Expected)
		{
			AllCorrect = false;
			AddError(FString::Printf(TEXT("Output[%d] = %d, expected %d"),
			                         NewIdx, OutputValues[NewIdx], Expected));
		}
	}

	TestTrue(TEXT("Index mapping parallel gather succeeded"), AllCorrect);

	return true;
}

// =============================================================================
// Reduction Pattern Tests
// =============================================================================

/**
 * Test thread-local accumulation with final reduction
 * (Alternative to atomic operations for sums/counts)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingReductionTest,
	"PCGEx.Functional.Threading.Reduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingReductionTest::RunTest(const FString& Parameters)
{
	const int32 NumElements = 10000;

	// Source data
	TArray<float> Values;
	Values.SetNumUninitialized(NumElements);
	for (int32 i = 0; i < NumElements; ++i)
	{
		Values[i] = static_cast<float>(i);
	}

	// Calculate expected sum (0 + 1 + 2 + ... + 9999)
	double ExpectedSum = static_cast<double>(NumElements - 1) * NumElements / 2.0;

	// Thread-local accumulation pattern
	// In real code, would use FThreadSafeCounter or per-thread buffers
	std::atomic<int64> AtomicSum{0};

	ParallelFor(NumElements, [&](int32 Index)
	{
		// Using atomic for this test - in practice, PCGEx uses per-thread buffers
		// or writes results to unique indices then reduces serially
		int64 IntValue = static_cast<int64>(Values[Index]);
		AtomicSum.fetch_add(IntValue, std::memory_order_relaxed);
	});

	double ActualSum = static_cast<double>(AtomicSum.load());

	TestTrue(TEXT("Parallel sum matches expected"),
	         FMath::IsNearlyEqual(ActualSum, ExpectedSum, 1.0));

	// Alternative: Calculate per-chunk then reduce
	const int32 NumChunks = 10;
	const int32 ChunkSize = NumElements / NumChunks;

	TArray<double> ChunkSums;
	ChunkSums.SetNumZeroed(NumChunks);

	ParallelFor(NumChunks, [&](int32 ChunkIndex)
	{
		int32 Start = ChunkIndex * ChunkSize;
		int32 End = (ChunkIndex == NumChunks - 1) ? NumElements : Start + ChunkSize;

		double LocalSum = 0;
		for (int32 i = Start; i < End; ++i)
		{
			LocalSum += Values[i];
		}

		// SAFE: Each chunk writes to unique index
		ChunkSums[ChunkIndex] = LocalSum;
	});

	// Serial reduction of chunk sums
	double ChunkTotal = 0;
	for (double ChunkSum : ChunkSums)
	{
		ChunkTotal += ChunkSum;
	}

	TestTrue(TEXT("Chunk-based parallel sum matches"),
	         FMath::IsNearlyEqual(ChunkTotal, ExpectedSum, 1.0));

	return true;
}

// =============================================================================
// Batch Processing Pattern Tests
// =============================================================================

/**
 * Test batch processing pattern (processing subsets of data)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExThreadingBatchProcessingTest,
	"PCGEx.Functional.Threading.BatchProcessing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExThreadingBatchProcessingTest::RunTest(const FString& Parameters)
{
	const int32 TotalItems = 1000;
	const int32 BatchSize = 100;
	const int32 NumBatches = (TotalItems + BatchSize - 1) / BatchSize;

	// Source data
	TArray<int32> Input;
	Input.SetNumUninitialized(TotalItems);
	for (int32 i = 0; i < TotalItems; ++i)
	{
		Input[i] = i;
	}

	// Output
	TArray<int32> Output;
	Output.SetNumUninitialized(TotalItems);

	// Process in batches (parallel batches, serial within batch)
	ParallelFor(NumBatches, [&](int32 BatchIndex)
	{
		int32 Start = BatchIndex * BatchSize;
		int32 End = FMath::Min(Start + BatchSize, TotalItems);

		// Serial processing within batch
		for (int32 i = Start; i < End; ++i)
		{
			// SAFE: Each batch processes unique range of indices
			Output[i] = Input[i] * 2;
		}
	});

	// Verify
	bool AllCorrect = true;
	for (int32 i = 0; i < TotalItems && AllCorrect; ++i)
	{
		if (Output[i] != i * 2)
		{
			AllCorrect = false;
			AddError(FString::Printf(TEXT("Output[%d] incorrect"), i));
		}
	}

	TestTrue(TEXT("Batch processing succeeded"), AllCorrect);

	return true;
}
