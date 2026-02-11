// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Union Data Unit Tests
 *
 * Tests IUnionData and FUnionMetadata:
 * - Element deduplication (TSet-based)
 * - Correct union size tracking
 * - Thread-safe add operations
 * - Metadata entry management
 * - The key bug fix: repeated InsertPoint for the same source point
 *   (e.g. a node appearing as endpoint of multiple edges) must NOT
 *   inflate union size.
 *
 * Test naming convention: PCGEx.Unit.Blending.UnionData.<Operation>
 */

#include "Misc/AutomationTest.h"
#include "Core/PCGExUnionData.h"
#include "Data/PCGExPointElements.h"

// =============================================================================
// FElement Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExElementEqualityTest,
	"PCGEx.Unit.Blending.UnionData.Element.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExElementEqualityTest::RunTest(const FString& Parameters)
{
	// Same Index + IO -> equal
	{
		PCGExData::FElement A(5, 2);
		PCGExData::FElement B(5, 2);
		TestTrue(TEXT("Elements with same Index and IO are equal"), A == B);
		TestEqual(TEXT("Equal elements have same hash"), GetTypeHash(A), GetTypeHash(B));
	}

	// Different Index -> not equal
	{
		PCGExData::FElement A(5, 2);
		PCGExData::FElement B(6, 2);
		TestFalse(TEXT("Elements with different Index are not equal"), A == B);
	}

	// Different IO -> not equal
	{
		PCGExData::FElement A(5, 2);
		PCGExData::FElement B(5, 3);
		TestFalse(TEXT("Elements with different IO are not equal"), A == B);
	}

	return true;
}

// =============================================================================
// IUnionData - Basic Operations
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataEmptyTest,
	"PCGEx.Unit.Blending.UnionData.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataEmptyTest::RunTest(const FString& Parameters)
{
	PCGExData::IUnionData Union;
	TestTrue(TEXT("New union is empty"), Union.IsEmpty());
	TestEqual(TEXT("New union has Num() == 0"), Union.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataAddSingleTest,
	"PCGEx.Unit.Blending.UnionData.AddSingle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataAddSingleTest::RunTest(const FString& Parameters)
{
	PCGExData::IUnionData Union;
	Union.Add_Unsafe(10, 0);

	TestFalse(TEXT("Union is not empty after add"), Union.IsEmpty());
	TestEqual(TEXT("Num() is 1 after single add"), Union.Num(), 1);
	TestTrue(TEXT("ContainsIO returns true for the IO index"), Union.ContainsIO(0));
	return true;
}

// =============================================================================
// IUnionData - Deduplication (key bug fix)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataDeduplicateTest,
	"PCGEx.Unit.Blending.UnionData.Deduplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataDeduplicateTest::RunTest(const FString& Parameters)
{
	// Adding the exact same element multiple times must NOT inflate the count.
	// This is the core fix: a node appearing as endpoint of N edges would
	// previously get Num()==N instead of 1.
	{
		PCGExData::IUnionData Union;
		Union.Add_Unsafe(5, 0);
		Union.Add_Unsafe(5, 0);
		Union.Add_Unsafe(5, 0);
		TestEqual(TEXT("Duplicate Add_Unsafe(Index,IO) must not inflate count"), Union.Num(), 1);
	}

	// Same test via FElement overload
	{
		PCGExData::IUnionData Union;
		PCGExData::FElement Elem(5, 0);
		Union.Add_Unsafe(Elem);
		Union.Add_Unsafe(Elem);
		Union.Add_Unsafe(Elem);
		TestEqual(TEXT("Duplicate Add_Unsafe(FElement) must not inflate count"), Union.Num(), 1);
	}

	// Thread-safe Add also deduplicates
	{
		PCGExData::IUnionData Union;
		Union.Add(5, 0);
		Union.Add(5, 0);
		TestEqual(TEXT("Duplicate thread-safe Add must not inflate count"), Union.Num(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataUniqueElementsTest,
	"PCGEx.Unit.Blending.UnionData.UniqueElements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataUniqueElementsTest::RunTest(const FString& Parameters)
{
	PCGExData::IUnionData Union;

	// Different point indices from same IO
	Union.Add_Unsafe(0, 0);
	Union.Add_Unsafe(1, 0);
	Union.Add_Unsafe(2, 0);
	TestEqual(TEXT("3 unique points from same IO -> Num()==3"), Union.Num(), 3);
	TestEqual(TEXT("GetIOSet has 1 unique IO"), Union.GetIOSet().Num(), 1);

	// Same point index from different IOs (genuine multi-source union)
	{
		PCGExData::IUnionData Union2;
		Union2.Add_Unsafe(0, 0);
		Union2.Add_Unsafe(0, 1);
		Union2.Add_Unsafe(0, 2);
		TestEqual(TEXT("Same point index from 3 different IOs -> Num()==3"), Union2.Num(), 3);
		TestEqual(TEXT("GetIOSet has 3 unique IOs"), Union2.GetIOSet().Num(), 3);
	}

	return true;
}

// =============================================================================
// IUnionData - Index -1 Normalization
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataNegativeIndexTest,
	"PCGEx.Unit.Blending.UnionData.NegativeIndexNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataNegativeIndexTest::RunTest(const FString& Parameters)
{
	// Index -1 is normalized to 0 in Add_Unsafe
	// So Add_Unsafe(-1, 0) and Add_Unsafe(0, 0) should be the same element
	PCGExData::IUnionData Union;
	Union.Add_Unsafe(-1, 0);
	Union.Add_Unsafe(0, 0);
	TestEqual(TEXT("Index -1 normalized to 0, deduplicates with explicit 0"), Union.Num(), 1);

	return true;
}

// =============================================================================
// IUnionData - Batch Add
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataBatchAddTest,
	"PCGEx.Unit.Blending.UnionData.BatchAdd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataBatchAddTest::RunTest(const FString& Parameters)
{
	// Batch add with unique indices
	{
		PCGExData::IUnionData Union;
		TArray<int32> Indices = {0, 1, 2, 3, 4};
		Union.Add_Unsafe(0, Indices);
		TestEqual(TEXT("Batch add of 5 unique indices -> Num()==5"), Union.Num(), 5);
		TestTrue(TEXT("ContainsIO returns true for IO 0"), Union.ContainsIO(0));
	}

	// Batch add with duplicates
	{
		PCGExData::IUnionData Union;
		TArray<int32> Indices = {0, 1, 0, 1, 0};
		Union.Add_Unsafe(0, Indices);
		TestEqual(TEXT("Batch add with duplicate indices deduplicates"), Union.Num(), 2);
	}

	return true;
}

// =============================================================================
// IUnionData - Reset
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataResetTest,
	"PCGEx.Unit.Blending.UnionData.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataResetTest::RunTest(const FString& Parameters)
{
	PCGExData::IUnionData Union;
	Union.Add_Unsafe(0, 0);
	Union.Add_Unsafe(1, 1);
	Union.Add_Unsafe(2, 2);
	TestEqual(TEXT("Pre-reset Num()==3"), Union.Num(), 3);

	Union.Reset();
	TestTrue(TEXT("After reset, IsEmpty"), Union.IsEmpty());
	TestEqual(TEXT("After reset, Num()==0"), Union.Num(), 0);
	TestEqual(TEXT("After reset, GetIOSet is empty"), Union.GetIOSet().Num(), 0);

	// Can add again after reset
	Union.Add_Unsafe(10, 5);
	TestEqual(TEXT("Can add after reset, Num()==1"), Union.Num(), 1);

	return true;
}

// =============================================================================
// IUnionData - Simulated Edge Insertion Scenario
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataEdgeInsertionScenarioTest,
	"PCGEx.Unit.Blending.UnionData.EdgeInsertionScenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataEdgeInsertionScenarioTest::RunTest(const FString& Parameters)
{
	// Simulates the real-world scenario that caused the bug:
	// A single source graph has a node (point index 5, IO 0) that is the
	// endpoint of 4 edges. InsertPoint is called once per edge, each time
	// Append-ing the same (5, 0) element. With the old TArray, Num() would
	// return 4. With the TSet fix, Num() correctly returns 1.

	PCGExData::IUnionData NodeUnion;
	const int32 PointIndex = 5;
	const int32 IOIndex = 0;

	// First call creates via NewEntry_Unsafe equivalent
	NodeUnion.Add_Unsafe(PointIndex, IOIndex);
	TestEqual(TEXT("After first edge, Num()==1"), NodeUnion.Num(), 1);

	// Three more edges share this node
	NodeUnion.Add_Unsafe(PointIndex, IOIndex);
	NodeUnion.Add_Unsafe(PointIndex, IOIndex);
	NodeUnion.Add_Unsafe(PointIndex, IOIndex);
	TestEqual(TEXT("After 4 edges sharing same node, Num() must still be 1"), NodeUnion.Num(), 1);

	// Now simulate a genuine union: same spatial position, different source
	const int32 OtherIOIndex = 1;
	NodeUnion.Add_Unsafe(PointIndex, OtherIOIndex);
	TestEqual(TEXT("After genuine union with 2nd source, Num()==2"), NodeUnion.Num(), 2);

	// The 2nd source also has multiple edges through this node
	NodeUnion.Add_Unsafe(PointIndex, OtherIOIndex);
	NodeUnion.Add_Unsafe(PointIndex, OtherIOIndex);
	TestEqual(TEXT("Repeated adds from 2nd source don't inflate, Num() still 2"), NodeUnion.Num(), 2);

	return true;
}

// =============================================================================
// FUnionMetadata - Entry Management
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionMetadataNewEntryTest,
	"PCGEx.Unit.Blending.UnionData.Metadata.NewEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionMetadataNewEntryTest::RunTest(const FString& Parameters)
{
	PCGExData::FUnionMetadata Metadata;
	TestEqual(TEXT("Fresh metadata has 0 entries"), Metadata.Num(), 0);

	// NewEntry_Unsafe requires a FConstPoint, but we can test via the simpler path
	// by using SetNum + NewEntryAt_Unsafe
	Metadata.SetNum(3);
	TestEqual(TEXT("After SetNum(3), Num()==3"), Metadata.Num(), 3);

	// Entries are nullptr until explicitly created
	TestFalse(TEXT("Entry 0 is nullptr before creation"), Metadata.Get(0).IsValid());

	TSharedPtr<PCGExData::IUnionData> Entry = Metadata.NewEntryAt_Unsafe(1);
	TestTrue(TEXT("NewEntryAt_Unsafe creates valid entry"), Entry.IsValid());
	TestTrue(TEXT("Get(1) returns valid entry"), Metadata.Get(1).IsValid());
	TestEqual(TEXT("New entry is empty"), Entry->Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionMetadataAppendTest,
	"PCGEx.Unit.Blending.UnionData.Metadata.Append",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionMetadataAppendTest::RunTest(const FString& Parameters)
{
	PCGExData::FUnionMetadata Metadata;
	Metadata.SetNum(2);

	// Create entry and manually add initial element
	TSharedPtr<PCGExData::IUnionData> Entry = Metadata.NewEntryAt_Unsafe(0);
	Entry->Add_Unsafe(10, 0);
	TestEqual(TEXT("Entry has 1 element after initial add"), Entry->Num(), 1);

	// Append same point (simulates repeated InsertPoint for same source)
	// Using FPoint which extends FElement
	PCGExData::FPoint SamePoint(10, 0);
	Metadata.Append_Unsafe(0, SamePoint);
	TestEqual(TEXT("Appending duplicate does not inflate count"), Entry->Num(), 1);

	// Append different point (genuine union)
	PCGExData::FPoint DiffPoint(20, 1);
	Metadata.Append_Unsafe(0, DiffPoint);
	TestEqual(TEXT("Appending unique point increases count to 2"), Entry->Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionMetadataGetBoundsTest,
	"PCGEx.Unit.Blending.UnionData.Metadata.GetBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionMetadataGetBoundsTest::RunTest(const FString& Parameters)
{
	PCGExData::FUnionMetadata Metadata;
	Metadata.SetNum(5);

	// Valid index but no entry created
	TestFalse(TEXT("Get(0) on uncreated entry returns nullptr"), Metadata.Get(0).IsValid());

	// Out of bounds
	TestFalse(TEXT("Get(-1) returns nullptr"), Metadata.Get(-1).IsValid());
	TestFalse(TEXT("Get(100) returns nullptr"), Metadata.Get(100).IsValid());

	// Valid entry
	Metadata.NewEntryAt_Unsafe(2);
	TestTrue(TEXT("Get(2) on created entry returns valid ptr"), Metadata.Get(2).IsValid());

	return true;
}

// =============================================================================
// FUnionMetadata - IOIndexOverlap
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionMetadataOverlapTest,
	"PCGEx.Unit.Blending.UnionData.Metadata.IOIndexOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionMetadataOverlapTest::RunTest(const FString& Parameters)
{
	PCGExData::FUnionMetadata Metadata;
	Metadata.SetNum(1);
	TSharedPtr<PCGExData::IUnionData> Entry = Metadata.NewEntryAt_Unsafe(0);
	Entry->Add_Unsafe(0, 2);
	Entry->Add_Unsafe(1, 5);
	Entry->Add_Unsafe(2, 7);

	// Overlap with {5} -> true
	{
		TSet<int32> TestSet = {5};
		TestTrue(TEXT("Overlaps with IO 5"), Metadata.IOIndexOverlap(0, TestSet));
	}

	// Overlap with {10, 20} -> false
	{
		TSet<int32> TestSet = {10, 20};
		TestFalse(TEXT("No overlap with IO {10,20}"), Metadata.IOIndexOverlap(0, TestSet));
	}

	// Overlap with {2, 7} -> true
	{
		TSet<int32> TestSet = {2, 7};
		TestTrue(TEXT("Overlaps with IO {2,7}"), Metadata.IOIndexOverlap(0, TestSet));
	}

	return true;
}

// =============================================================================
// IUnionData - Reserve
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExUnionDataReserveTest,
	"PCGEx.Unit.Blending.UnionData.Reserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExUnionDataReserveTest::RunTest(const FString& Parameters)
{
	// Just verify Reserve doesn't crash and elements still work after
	PCGExData::IUnionData Union;
	Union.Reserve(16, 32);
	Union.Add_Unsafe(0, 0);
	Union.Add_Unsafe(1, 1);
	TestEqual(TEXT("After reserve + adds, Num()==2"), Union.Num(), 2);

	// Reserve with small values (below inline threshold) should be fine too
	PCGExData::IUnionData Union2;
	Union2.Reserve(2, 4);
	Union2.Add_Unsafe(0, 0);
	TestEqual(TEXT("Small reserve + add works"), Union2.Num(), 1);

	return true;
}
