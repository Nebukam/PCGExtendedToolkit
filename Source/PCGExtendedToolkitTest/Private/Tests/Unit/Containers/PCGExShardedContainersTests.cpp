// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Sharded Containers Unit Tests
 *
 * Tests thread-safe sharded containers:
 * - TH64SetShards: Sharded hash set for concurrent access
 * - TH64MapShards: Sharded hash map for concurrent access
 *
 * These containers distribute data across multiple shards using
 * a hash function, allowing concurrent access with reduced lock contention.
 *
 * Test naming convention: PCGEx.Unit.Containers.Sharded.<Type>.<Operation>
 */

#include "Misc/AutomationTest.h"
#include "Containers/PCGExScopedContainers.h"

// =============================================================================
// TH64SetShards Tests
// =============================================================================

/**
 * Test TH64SetShards basic operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsBasicTest,
	"PCGEx.Unit.Containers.Sharded.Set.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsBasicTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	// Add values
	Set.Add(100);
	Set.Add(200);
	Set.Add(300);

	// Contains should return true for added values
	TestTrue(TEXT("Contains(100) is true"), Set.Contains(100));
	TestTrue(TEXT("Contains(200) is true"), Set.Contains(200));
	TestTrue(TEXT("Contains(300) is true"), Set.Contains(300));

	// Contains should return false for non-added values
	TestFalse(TEXT("Contains(400) is false"), Set.Contains(400));
	TestFalse(TEXT("Contains(0) is false"), Set.Contains(0));

	return true;
}

/**
 * Test TH64SetShards Add with bIsAlreadySet output
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsAddDuplicateTest,
	"PCGEx.Unit.Containers.Sharded.Set.AddDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsAddDuplicateTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	bool bIsAlreadySet = true; // Start with true to verify it's set to false

	// First add - should not be already set
	Set.Add(12345, bIsAlreadySet);
	TestFalse(TEXT("First add: bIsAlreadySet is false"), bIsAlreadySet);

	// Second add of same value - should be already set
	Set.Add(12345, bIsAlreadySet);
	TestTrue(TEXT("Second add: bIsAlreadySet is true"), bIsAlreadySet);

	// Add different value - should not be already set
	Set.Add(67890, bIsAlreadySet);
	TestFalse(TEXT("Different value: bIsAlreadySet is false"), bIsAlreadySet);

	return true;
}

/**
 * Test TH64SetShards Remove
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsRemoveTest,
	"PCGEx.Unit.Containers.Sharded.Set.Remove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsRemoveTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	Set.Add(100);
	Set.Add(200);

	TestTrue(TEXT("Contains 100 before remove"), Set.Contains(100));

	// Remove returns count of removed items
	int32 Removed = Set.Remove(100);
	TestEqual(TEXT("Remove returns 1"), Removed, 1);
	TestFalse(TEXT("Contains 100 after remove is false"), Set.Contains(100));
	TestTrue(TEXT("Contains 200 still true"), Set.Contains(200));

	// Remove non-existent value
	Removed = Set.Remove(999);
	TestEqual(TEXT("Remove non-existent returns 0"), Removed, 0);

	return true;
}

/**
 * Test TH64SetShards Collapse
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsCollapseTest,
	"PCGEx.Unit.Containers.Sharded.Set.Collapse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsCollapseTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	// Add several values
	Set.Add(1);
	Set.Add(2);
	Set.Add(3);
	Set.Add(100);
	Set.Add(200);

	// Collapse into a single set
	TSet<uint64> Merged;
	Set.Collapse(Merged);

	// All values should be in merged set
	TestEqual(TEXT("Merged set has 5 elements"), Merged.Num(), 5);
	TestTrue(TEXT("Merged contains 1"), Merged.Contains(1));
	TestTrue(TEXT("Merged contains 2"), Merged.Contains(2));
	TestTrue(TEXT("Merged contains 3"), Merged.Contains(3));
	TestTrue(TEXT("Merged contains 100"), Merged.Contains(100));
	TestTrue(TEXT("Merged contains 200"), Merged.Contains(200));

	// After collapse, shards should be empty
	TestFalse(TEXT("Sharded set no longer contains 1"), Set.Contains(1));

	return true;
}

/**
 * Test TH64SetShards Empty
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsEmptyTest,
	"PCGEx.Unit.Containers.Sharded.Set.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsEmptyTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	Set.Add(1);
	Set.Add(2);
	Set.Add(3);

	TestTrue(TEXT("Contains 1 before empty"), Set.Contains(1));

	Set.Empty();

	TestFalse(TEXT("Contains 1 after empty"), Set.Contains(1));
	TestFalse(TEXT("Contains 2 after empty"), Set.Contains(2));
	TestFalse(TEXT("Contains 3 after empty"), Set.Contains(3));

	return true;
}

/**
 * Test TH64SetShards with large values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64SetShardsLargeValuesTest,
	"PCGEx.Unit.Containers.Sharded.Set.LargeValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64SetShardsLargeValuesTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64SetShards<32> Set;

	// Test with large uint64 values
	const uint64 LargeVal1 = 0xFFFFFFFFFFFFFFFFull;
	const uint64 LargeVal2 = 0x8000000000000000ull;
	const uint64 LargeVal3 = 0x123456789ABCDEFull;

	Set.Add(LargeVal1);
	Set.Add(LargeVal2);
	Set.Add(LargeVal3);

	TestTrue(TEXT("Contains max uint64"), Set.Contains(LargeVal1));
	TestTrue(TEXT("Contains large signed-like value"), Set.Contains(LargeVal2));
	TestTrue(TEXT("Contains specific large value"), Set.Contains(LargeVal3));

	return true;
}

// =============================================================================
// TH64MapShards Tests
// =============================================================================

/**
 * Test TH64MapShards basic operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64MapShardsBasicTest,
	"PCGEx.Unit.Containers.Sharded.Map.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64MapShardsBasicTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64MapShards<int32, 32> Map;

	// Add key-value pairs
	Map.Add(100, 1000);
	Map.Add(200, 2000);
	Map.Add(300, 3000);

	// Contains should work
	TestTrue(TEXT("Contains(100) is true"), Map.Contains(100));
	TestTrue(TEXT("Contains(200) is true"), Map.Contains(200));
	TestFalse(TEXT("Contains(400) is false"), Map.Contains(400));

	// Find should return correct values
	const int32* Value100 = Map.Find(100);
	TestNotNull(TEXT("Find(100) not null"), Value100);
	if (Value100)
	{
		TestEqual(TEXT("Find(100) = 1000"), *Value100, 1000);
	}

	const int32* Value200 = Map.Find(200);
	TestNotNull(TEXT("Find(200) not null"), Value200);
	if (Value200)
	{
		TestEqual(TEXT("Find(200) = 2000"), *Value200, 2000);
	}

	// Find non-existent should return null
	const int32* Value400 = Map.Find(400);
	TestNull(TEXT("Find(400) is null"), Value400);

	return true;
}

/**
 * Test TH64MapShards Remove
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64MapShardsRemoveTest,
	"PCGEx.Unit.Containers.Sharded.Map.Remove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64MapShardsRemoveTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64MapShards<FString, 32> Map;

	Map.Add(1, TEXT("One"));
	Map.Add(2, TEXT("Two"));

	TestTrue(TEXT("Contains 1 before remove"), Map.Contains(1));

	int32 Removed = Map.Remove(1);
	TestEqual(TEXT("Remove returns 1"), Removed, 1);
	TestFalse(TEXT("Contains 1 after remove"), Map.Contains(1));
	TestTrue(TEXT("Contains 2 still"), Map.Contains(2));

	return true;
}

/**
 * Test TH64MapShards FindOrAddAndUpdate
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64MapShardsFindOrAddTest,
	"PCGEx.Unit.Containers.Sharded.Map.FindOrAddAndUpdate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64MapShardsFindOrAddTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64MapShards<int32, 32> Map;

	// First call - should create new entry
	bool WasNew = false;
	Map.FindOrAddAndUpdate(100, 0, [&WasNew](int32& Value, bool bIsNew)
	{
		WasNew = bIsNew;
		Value = 42;
	});

	TestTrue(TEXT("First call: was new"), WasNew);
	const int32* Val = Map.Find(100);
	TestNotNull(TEXT("Value exists"), Val);
	if (Val)
	{
		TestEqual(TEXT("Value is 42"), *Val, 42);
	}

	// Second call - should update existing entry
	Map.FindOrAddAndUpdate(100, 0, [&WasNew](int32& Value, bool bIsNew)
	{
		WasNew = bIsNew;
		Value += 10;
	});

	TestFalse(TEXT("Second call: was not new"), WasNew);
	Val = Map.Find(100);
	if (Val)
	{
		TestEqual(TEXT("Value is now 52"), *Val, 52);
	}

	return true;
}

/**
 * Test TH64MapShards Collapse
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64MapShardsCollapseTest,
	"PCGEx.Unit.Containers.Sharded.Map.Collapse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64MapShardsCollapseTest::RunTest(const FString& Parameters)
{
	PCGExMT::TH64MapShards<double, 32> Map;

	Map.Add(1, 1.1);
	Map.Add(2, 2.2);
	Map.Add(3, 3.3);

	TMap<uint64, double> Merged;
	Map.Collapse(Merged);

	TestEqual(TEXT("Merged map has 3 elements"), Merged.Num(), 3);

	const double* V1 = Merged.Find(1);
	TestNotNull(TEXT("Merged contains key 1"), V1);
	if (V1)
	{
		TestTrue(TEXT("Value for key 1 is 1.1"),
		         FMath::IsNearlyEqual(*V1, 1.1, 0.01));
	}

	// After collapse, sharded map should be empty
	TestFalse(TEXT("Sharded map no longer contains 1"), Map.Contains(1));

	return true;
}

/**
 * Test TH64MapShards with different value types
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExH64MapShardsValueTypesTest,
	"PCGEx.Unit.Containers.Sharded.Map.ValueTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExH64MapShardsValueTypesTest::RunTest(const FString& Parameters)
{
	// Test with FVector values
	{
		PCGExMT::TH64MapShards<FVector, 32> VectorMap;
		VectorMap.Add(1, FVector(1, 2, 3));
		VectorMap.Add(2, FVector(4, 5, 6));

		const FVector* V = VectorMap.Find(1);
		TestNotNull(TEXT("FVector: Find(1) not null"), V);
		if (V)
		{
			TestTrue(TEXT("FVector: value equals (1,2,3)"),
			         V->Equals(FVector(1, 2, 3), KINDA_SMALL_NUMBER));
		}
	}

	// Test with TArray values
	{
		PCGExMT::TH64MapShards<TArray<int32>, 32> ArrayMap;

		TArray<int32> Arr1 = {1, 2, 3};
		TArray<int32> Arr2 = {4, 5, 6, 7};
		ArrayMap.Add(100, Arr1);
		ArrayMap.Add(200, Arr2);

		const TArray<int32>* A = ArrayMap.Find(100);
		TestNotNull(TEXT("TArray: Find(100) not null"), A);
		if (A)
		{
			TestEqual(TEXT("TArray: value has 3 elements"), A->Num(), 3);
			TestEqual(TEXT("TArray: first element is 1"), (*A)[0], 1);
		}
	}

	return true;
}

// =============================================================================
// Hash Distribution Tests
// =============================================================================

/**
 * Test that values are distributed across shards
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExShardedDistributionTest,
	"PCGEx.Unit.Containers.Sharded.Distribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExShardedDistributionTest::RunTest(const FString& Parameters)
{
	// Test that sequential values don't all end up in the same shard
	// We can verify this indirectly by checking that operations work correctly
	// for a range of sequential values

	PCGExMT::TH64SetShards<32> Set;

	// Add 1000 sequential values
	for (uint64 i = 0; i < 1000; ++i)
	{
		Set.Add(i);
	}

	// All values should be retrievable
	bool AllFound = true;
	for (uint64 i = 0; i < 1000; ++i)
	{
		if (!Set.Contains(i))
		{
			AllFound = false;
			AddError(FString::Printf(TEXT("Value %llu not found"), i));
			break;
		}
	}

	TestTrue(TEXT("All 1000 sequential values found"), AllFound);

	// Collapse should return all values
	TSet<uint64> Merged;
	Set.Collapse(Merged);
	TestEqual(TEXT("Collapsed set has 1000 elements"), Merged.Num(), 1000);

	return true;
}
