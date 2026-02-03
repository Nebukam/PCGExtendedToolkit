// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExIndexLookup Unit Tests
 *
 * Tests FIndexLookup container functionality:
 * - Construction and initialization
 * - Get/Set operations
 * - Operator overloads
 * - Array view conversion
 *
 * FIndexLookup is a simple wrapper around TArray<int32> that initializes
 * all values to -1, commonly used for index mapping/remapping operations.
 *
 * Test naming convention: PCGEx.Unit.Containers.IndexLookup.<Operation>
 */

#include "Misc/AutomationTest.h"
#include "Containers/PCGExIndexLookup.h"

using namespace PCGEx;

// =============================================================================
// Construction Tests
// =============================================================================

/**
 * Test FIndexLookup construction and initialization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupConstructionTest,
	"PCGEx.Unit.Containers.IndexLookup.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupConstructionTest::RunTest(const FString& Parameters)
{
	// Test basic construction with size
	{
		FIndexLookup Lookup(10);

		// All values should be initialized to -1
		for (int32 i = 0; i < 10; ++i)
		{
			TestEqual(FString::Printf(TEXT("Initial value at %d should be -1"), i),
			          Lookup[i], -1);
		}
	}

	// Test construction with different sizes
	{
		FIndexLookup Small(1);
		TestEqual(TEXT("Single element lookup initialized to -1"), Small[0], -1);

		FIndexLookup Large(1000);
		TestEqual(TEXT("Large lookup first element initialized to -1"), Large[0], -1);
		TestEqual(TEXT("Large lookup last element initialized to -1"), Large[999], -1);
	}

	// Test construction with bFill parameter (currently same behavior)
	{
		FIndexLookup Filled(5, true);
		for (int32 i = 0; i < 5; ++i)
		{
			TestEqual(FString::Printf(TEXT("Filled value at %d should be -1"), i),
			          Filled[i], -1);
		}
	}

	return true;
}

// =============================================================================
// Get/Set Operations Tests
// =============================================================================

/**
 * Test Get and Set methods
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupGetSetTest,
	"PCGEx.Unit.Containers.IndexLookup.GetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupGetSetTest::RunTest(const FString& Parameters)
{
	FIndexLookup Lookup(10);

	// Test Set method
	Lookup.Set(0, 100);
	Lookup.Set(5, 500);
	Lookup.Set(9, 999);

	// Test Get method
	TestEqual(TEXT("Get(0) returns set value"), Lookup.Get(0), 100);
	TestEqual(TEXT("Get(5) returns set value"), Lookup.Get(5), 500);
	TestEqual(TEXT("Get(9) returns set value"), Lookup.Get(9), 999);

	// Unchanged values should still be -1
	TestEqual(TEXT("Get(1) returns -1 (unchanged)"), Lookup.Get(1), -1);
	TestEqual(TEXT("Get(4) returns -1 (unchanged)"), Lookup.Get(4), -1);

	// Test overwriting values
	Lookup.Set(5, 555);
	TestEqual(TEXT("Set overwrites previous value"), Lookup.Get(5), 555);

	// Test negative values (valid use case for marking special states)
	Lookup.Set(3, -999);
	TestEqual(TEXT("Negative values can be stored"), Lookup.Get(3), -999);

	return true;
}

/**
 * Test GetMutable for reference access
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupGetMutableTest,
	"PCGEx.Unit.Containers.IndexLookup.GetMutable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupGetMutableTest::RunTest(const FString& Parameters)
{
	FIndexLookup Lookup(5);

	// Modify through GetMutable reference
	Lookup.GetMutable(2) = 42;
	TestEqual(TEXT("GetMutable allows direct assignment"), Lookup.Get(2), 42);

	// Increment through reference
	Lookup.GetMutable(2)++;
	TestEqual(TEXT("GetMutable allows increment"), Lookup.Get(2), 43);

	// Use in compound operations
	Lookup.GetMutable(0) += 10;
	TestEqual(TEXT("GetMutable allows compound assignment (started from -1)"),
	          Lookup.Get(0), 9); // -1 + 10 = 9

	return true;
}

// =============================================================================
// Operator Overload Tests
// =============================================================================

/**
 * Test operator[] overloads
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupOperatorTest,
	"PCGEx.Unit.Containers.IndexLookup.Operators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupOperatorTest::RunTest(const FString& Parameters)
{
	FIndexLookup Lookup(5);

	// Non-const operator[] for write access
	Lookup[0] = 100;
	Lookup[1] = 200;
	Lookup[2] = 300;

	// Const operator[] for read access
	const FIndexLookup& ConstRef = Lookup;
	TestEqual(TEXT("Const operator[] reads correctly"), ConstRef[0], 100);
	TestEqual(TEXT("Const operator[] reads correctly"), ConstRef[1], 200);
	TestEqual(TEXT("Const operator[] reads correctly"), ConstRef[2], 300);
	TestEqual(TEXT("Const operator[] returns -1 for unset"), ConstRef[3], -1);

	// Non-const access for chained operations
	Lookup[4] = Lookup[0] + Lookup[1];
	TestEqual(TEXT("Chained operator[] works"), Lookup[4], 300);

	return true;
}

// =============================================================================
// Array View Conversion Tests
// =============================================================================

/**
 * Test conversion to TArrayView
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupArrayViewTest,
	"PCGEx.Unit.Containers.IndexLookup.ArrayView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupArrayViewTest::RunTest(const FString& Parameters)
{
	FIndexLookup Lookup(5);
	Lookup.Set(0, 10);
	Lookup.Set(1, 20);
	Lookup.Set(2, 30);

	// Test const array view conversion
	TArrayView<const int32> ConstView = Lookup;
	TestEqual(TEXT("ConstView Num() matches"), ConstView.Num(), 5);
	TestEqual(TEXT("ConstView[0] matches"), ConstView[0], 10);
	TestEqual(TEXT("ConstView[1] matches"), ConstView[1], 20);
	TestEqual(TEXT("ConstView[4] is -1"), ConstView[4], -1);

	// Test mutable array view conversion
	TArrayView<int32> MutableView = Lookup;
	TestEqual(TEXT("MutableView Num() matches"), MutableView.Num(), 5);

	// Modify through view
	MutableView[3] = 40;
	TestEqual(TEXT("Modification through MutableView reflects in Lookup"),
	          Lookup.Get(3), 40);

	// Iterate using range-for on view
	int32 Sum = 0;
	for (int32 Value : ConstView)
	{
		if (Value > 0) Sum += Value;
	}
	TestEqual(TEXT("Range-for iteration works"), Sum, 100); // 10+20+30+40

	return true;
}

// =============================================================================
// Use Case Tests
// =============================================================================

/**
 * Test common use case: index remapping
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupRemappingTest,
	"PCGEx.Unit.Containers.IndexLookup.UseCase.Remapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupRemappingTest::RunTest(const FString& Parameters)
{
	// Simulate remapping indices after filtering
	// Original indices: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
	// After filter (keeping even): 0, 2, 4, 6, 8
	// New indices:                  0, 1, 2, 3, 4

	const int32 OriginalCount = 10;
	FIndexLookup OldToNew(OriginalCount);

	int32 NewIndex = 0;
	for (int32 OldIndex = 0; OldIndex < OriginalCount; ++OldIndex)
	{
		if (OldIndex % 2 == 0) // Keep only even indices
		{
			OldToNew.Set(OldIndex, NewIndex);
			NewIndex++;
		}
		// Odd indices stay at -1 (filtered out)
	}

	// Verify mapping
	TestEqual(TEXT("Old index 0 maps to new index 0"), OldToNew[0], 0);
	TestEqual(TEXT("Old index 1 filtered out"), OldToNew[1], -1);
	TestEqual(TEXT("Old index 2 maps to new index 1"), OldToNew[2], 1);
	TestEqual(TEXT("Old index 3 filtered out"), OldToNew[3], -1);
	TestEqual(TEXT("Old index 8 maps to new index 4"), OldToNew[8], 4);

	// Common pattern: check if index is valid before use
	auto GetNewIndex = [&OldToNew](int32 OldIdx) -> int32
	{
		const int32 NewIdx = OldToNew[OldIdx];
		return NewIdx; // Returns -1 if filtered
	};

	TestEqual(TEXT("Helper function returns valid mapping"), GetNewIndex(4), 2);
	TestEqual(TEXT("Helper function returns -1 for filtered"), GetNewIndex(5), -1);

	return true;
}

/**
 * Test use case: tracking visited/processed indices
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexLookupVisitedTest,
	"PCGEx.Unit.Containers.IndexLookup.UseCase.Visited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExIndexLookupVisitedTest::RunTest(const FString& Parameters)
{
	// Use -1 as "not visited", any other value as "visited with order"
	const int32 Count = 10;
	FIndexLookup VisitOrder(Count);

	// Simulate visiting indices in a specific order
	TArray<int32> VisitSequence = {5, 2, 8, 0, 7};
	for (int32 Order = 0; Order < VisitSequence.Num(); ++Order)
	{
		const int32 Index = VisitSequence[Order];
		VisitOrder.Set(Index, Order);
	}

	// Check visited status
	auto IsVisited = [&VisitOrder](int32 Idx) -> bool
	{
		return VisitOrder[Idx] >= 0;
	};

	auto GetVisitOrder = [&VisitOrder](int32 Idx) -> int32
	{
		return VisitOrder[Idx];
	};

	TestTrue(TEXT("Index 5 was visited"), IsVisited(5));
	TestEqual(TEXT("Index 5 was visited first"), GetVisitOrder(5), 0);

	TestTrue(TEXT("Index 0 was visited"), IsVisited(0));
	TestEqual(TEXT("Index 0 was visited fourth"), GetVisitOrder(0), 3);

	TestFalse(TEXT("Index 1 was not visited"), IsVisited(1));
	TestFalse(TEXT("Index 9 was not visited"), IsVisited(9));

	return true;
}
