// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExHashLookup Unit Tests
 *
 * Tests hash lookup containers:
 * - FHashLookup (abstract base)
 * - FHashLookupArray (dense array-based storage)
 * - FHashLookupMap (sparse map-based storage)
 *
 * These containers map int32 indices to uint64 hash values, with a
 * configurable "init value" that represents "no value set".
 *
 * Test naming convention: PCGEx.Unit.Containers.HashLookup.<Type>.<Operation>
 */

#include "Misc/AutomationTest.h"
#include "Containers/PCGExHashLookup.h"

using namespace PCGEx;

// =============================================================================
// FHashLookupArray Tests
// =============================================================================

/**
 * Test FHashLookupArray construction and initialization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupArrayConstructionTest,
	"PCGEx.Unit.Containers.HashLookup.Array.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupArrayConstructionTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 0xDEADBEEF;
	const int32 Size = 10;

	FHashLookupArray Lookup(InitValue, Size);

	// All values should be initialized to InitValue
	for (int32 i = 0; i < Size; ++i)
	{
		TestEqual(FString::Printf(TEXT("Initial value at %d should be InitValue"), i),
		          Lookup.Get(i), InitValue);
	}

	// Test IsInitValue
	TestTrue(TEXT("InitValue is recognized"), Lookup.IsInitValue(InitValue));
	TestFalse(TEXT("Other values are not init value"), Lookup.IsInitValue(12345));

	return true;
}

/**
 * Test FHashLookupArray Get/Set operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupArrayGetSetTest,
	"PCGEx.Unit.Containers.HashLookup.Array.GetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupArrayGetSetTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 0;
	FHashLookupArray Lookup(InitValue, 10);

	// Set some values
	Lookup.Set(0, 100);
	Lookup.Set(5, 500);
	Lookup.Set(9, 999);

	// Get values
	TestEqual(TEXT("Get(0) returns set value"), Lookup.Get(0), static_cast<uint64>(100));
	TestEqual(TEXT("Get(5) returns set value"), Lookup.Get(5), static_cast<uint64>(500));
	TestEqual(TEXT("Get(9) returns set value"), Lookup.Get(9), static_cast<uint64>(999));

	// Unset values should return InitValue
	TestEqual(TEXT("Get(1) returns InitValue"), Lookup.Get(1), InitValue);
	TestEqual(TEXT("Get(4) returns InitValue"), Lookup.Get(4), InitValue);

	// Large hash values
	const uint64 LargeHash = 0xFFFFFFFFFFFFFFFFull;
	Lookup.Set(3, LargeHash);
	TestEqual(TEXT("Large hash values stored correctly"), Lookup.Get(3), LargeHash);

	return true;
}

/**
 * Test FHashLookupArray Reset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupArrayResetTest,
	"PCGEx.Unit.Containers.HashLookup.Array.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupArrayResetTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 42;
	FHashLookupArray Lookup(InitValue, 5);

	// Set all values to something different
	for (int32 i = 0; i < 5; ++i)
	{
		Lookup.Set(i, static_cast<uint64>(i * 100));
	}

	// Verify they changed
	TestEqual(TEXT("Value changed before reset"), Lookup.Get(2), static_cast<uint64>(200));

	// Reset
	Lookup.Reset();

	// All values should be back to InitValue
	for (int32 i = 0; i < 5; ++i)
	{
		TestEqual(FString::Printf(TEXT("Value at %d reset to InitValue"), i),
		          Lookup.Get(i), InitValue);
	}

	return true;
}

/**
 * Test FHashLookupArray view conversions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupArrayViewTest,
	"PCGEx.Unit.Containers.HashLookup.Array.View",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupArrayViewTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 0;
	FHashLookupArray Lookup(InitValue, 5);

	Lookup.Set(0, 10);
	Lookup.Set(1, 20);
	Lookup.Set(2, 30);

	// Const view
	TArrayView<const uint64> ConstView = Lookup;
	TestEqual(TEXT("ConstView Num() matches"), ConstView.Num(), 5);
	TestEqual(TEXT("ConstView[0] matches"), ConstView[0], static_cast<uint64>(10));
	TestEqual(TEXT("ConstView[4] is InitValue"), ConstView[4], InitValue);

	// Mutable view
	TArrayView<uint64> MutableView = Lookup;
	MutableView[3] = 40;
	TestEqual(TEXT("Modification through view reflects"), Lookup.Get(3), static_cast<uint64>(40));

	return true;
}

// =============================================================================
// FHashLookupMap Tests
// =============================================================================

/**
 * Test FHashLookupMap construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupMapConstructionTest,
	"PCGEx.Unit.Containers.HashLookup.Map.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupMapConstructionTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 0xDEADBEEF;

	// Map-based lookup doesn't pre-allocate, just reserves
	FHashLookupMap Lookup(InitValue, 100);

	// Getting unset values should return InitValue
	TestEqual(TEXT("Unset key returns InitValue"), Lookup.Get(50), InitValue);
	TestTrue(TEXT("InitValue recognized"), Lookup.IsInitValue(InitValue));

	return true;
}

/**
 * Test FHashLookupMap Get/Set operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupMapGetSetTest,
	"PCGEx.Unit.Containers.HashLookup.Map.GetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupMapGetSetTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 0;
	FHashLookupMap Lookup(InitValue, 0);

	// Sparse setting - only specific indices
	Lookup.Set(100, 1000);
	Lookup.Set(50000, 50000000);
	Lookup.Set(0, 1);

	// Get values
	TestEqual(TEXT("Get(100) returns set value"), Lookup.Get(100), static_cast<uint64>(1000));
	TestEqual(TEXT("Get(50000) returns set value"), Lookup.Get(50000), static_cast<uint64>(50000000));
	TestEqual(TEXT("Get(0) returns set value"), Lookup.Get(0), static_cast<uint64>(1));

	// Unset values return InitValue
	TestEqual(TEXT("Get(1) returns InitValue"), Lookup.Get(1), InitValue);
	TestEqual(TEXT("Get(99999) returns InitValue"), Lookup.Get(99999), InitValue);

	// Contains check
	TestTrue(TEXT("Contains(100) is true"), Lookup.Contains(100));
	TestTrue(TEXT("Contains(50000) is true"), Lookup.Contains(50000));
	TestFalse(TEXT("Contains(1) is false"), Lookup.Contains(1));
	TestFalse(TEXT("Contains(99) is false"), Lookup.Contains(99));

	return true;
}

/**
 * Test FHashLookupMap Reset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupMapResetTest,
	"PCGEx.Unit.Containers.HashLookup.Map.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupMapResetTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 42;
	FHashLookupMap Lookup(InitValue, 0);

	// Set some values
	Lookup.Set(10, 100);
	Lookup.Set(20, 200);
	Lookup.Set(30, 300);

	TestTrue(TEXT("Contains(10) before reset"), Lookup.Contains(10));

	// Reset
	Lookup.Reset();

	// Map should be empty
	TestFalse(TEXT("Contains(10) after reset"), Lookup.Contains(10));
	TestFalse(TEXT("Contains(20) after reset"), Lookup.Contains(20));
	TestEqual(TEXT("Get returns InitValue after reset"), Lookup.Get(10), InitValue);

	return true;
}

// =============================================================================
// Factory Function Tests
// =============================================================================

/**
 * Test NewHashLookup factory function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupFactoryTest,
	"PCGEx.Unit.Containers.HashLookup.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupFactoryTest::RunTest(const FString& Parameters)
{
	const uint64 InitValue = 123456;
	const int32 Size = 100;

	// Create array-based lookup
	TSharedPtr<FHashLookup> ArrayLookup = NewHashLookup<FHashLookupArray>(InitValue, Size);
	TestTrue(TEXT("Array factory creates valid pointer"), ArrayLookup.IsValid());
	TestEqual(TEXT("Array lookup returns InitValue"), ArrayLookup->Get(50), InitValue);

	// Create map-based lookup
	TSharedPtr<FHashLookup> MapLookup = NewHashLookup<FHashLookupMap>(InitValue, Size);
	TestTrue(TEXT("Map factory creates valid pointer"), MapLookup.IsValid());
	TestEqual(TEXT("Map lookup returns InitValue"), MapLookup->Get(50), InitValue);

	// Both should work with polymorphic interface
	ArrayLookup->Set(10, 999);
	MapLookup->Set(10, 999);

	TestEqual(TEXT("Array polymorphic Set/Get works"), ArrayLookup->Get(10), static_cast<uint64>(999));
	TestEqual(TEXT("Map polymorphic Set/Get works"), MapLookup->Get(10), static_cast<uint64>(999));

	return true;
}

// =============================================================================
// Use Case Tests
// =============================================================================

/**
 * Test common use case: hash-based deduplication
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupDeduplicationTest,
	"PCGEx.Unit.Containers.HashLookup.UseCase.Deduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupDeduplicationTest::RunTest(const FString& Parameters)
{
	// Simulate deduplicating points based on position hash
	// Use array lookup for dense index space

	const uint64 NoHash = 0; // Init value meaning "not hashed yet"
	FHashLookupArray PositionHashes(NoHash, 10);

	// Simulate computing hashes for positions
	auto ComputePositionHash = [](const FVector& Pos) -> uint64
	{
		// Simple hash for testing - real code would use proper spatial hash
		return static_cast<uint64>(
			FMath::RoundToInt(Pos.X * 100) ^
			(FMath::RoundToInt(Pos.Y * 100) << 20) ^
			(FMath::RoundToInt(Pos.Z * 100) << 40)
		);
	};

	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(0, 0, 0),   // Duplicate of index 0
		FVector(2, 0, 0),
		FVector(1, 0, 0),   // Duplicate of index 1
	};

	// Track unique positions
	TMap<uint64, int32> HashToFirstIndex;
	TArray<int32> UniqueIndices;

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		uint64 Hash = ComputePositionHash(Positions[i]);
		PositionHashes.Set(i, Hash);

		if (!HashToFirstIndex.Contains(Hash))
		{
			HashToFirstIndex.Add(Hash, i);
			UniqueIndices.Add(i);
		}
	}

	// Verify unique count
	TestEqual(TEXT("3 unique positions found"), UniqueIndices.Num(), 3);

	// Verify which indices are unique
	TestTrue(TEXT("Index 0 is unique"), UniqueIndices.Contains(0));
	TestTrue(TEXT("Index 1 is unique"), UniqueIndices.Contains(1));
	TestFalse(TEXT("Index 2 is duplicate"), UniqueIndices.Contains(2));
	TestTrue(TEXT("Index 3 is unique"), UniqueIndices.Contains(3));
	TestFalse(TEXT("Index 4 is duplicate"), UniqueIndices.Contains(4));

	return true;
}

/**
 * Test use case: sparse index tracking with map
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExHashLookupSparseTrackingTest,
	"PCGEx.Unit.Containers.HashLookup.UseCase.SparseTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExHashLookupSparseTrackingTest::RunTest(const FString& Parameters)
{
	// Use map-based lookup for sparse data where indices are spread out
	// Example: tracking which edge indices connect to which node

	const uint64 NoConnection = MAX_uint64;
	FHashLookupMap EdgeToNode(NoConnection, 0);

	// Simulate sparse edge assignments
	// Edge indices might be large and non-contiguous
	EdgeToNode.Set(100, 0);    // Edge 100 connects to node 0
	EdgeToNode.Set(5000, 1);   // Edge 5000 connects to node 1
	EdgeToNode.Set(99999, 2);  // Edge 99999 connects to node 2

	// Query connections
	auto GetConnectedNode = [&EdgeToNode, NoConnection](int32 EdgeIndex) -> int32
	{
		uint64 NodeHash = EdgeToNode.Get(EdgeIndex);
		return NodeHash == NoConnection ? -1 : static_cast<int32>(NodeHash);
	};

	TestEqual(TEXT("Edge 100 connects to node 0"), GetConnectedNode(100), 0);
	TestEqual(TEXT("Edge 5000 connects to node 1"), GetConnectedNode(5000), 1);
	TestEqual(TEXT("Edge 99999 connects to node 2"), GetConnectedNode(99999), 2);
	TestEqual(TEXT("Edge 50 has no connection"), GetConnectedNode(50), -1);
	TestEqual(TEXT("Edge 10000 has no connection"), GetConnectedNode(10000), -1);

	// Memory efficiency: map only stores 3 entries despite large index range
	// Array-based would need 100,000 entries

	return true;
}
