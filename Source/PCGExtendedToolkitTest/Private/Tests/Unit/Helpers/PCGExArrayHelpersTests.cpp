// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExArrayHelpers Unit Tests
 *
 * Tests array manipulation and string parsing from PCGExArrayHelpers.h:
 * - GetStringArrayFromCommaSeparatedList: Parse comma-separated strings
 * - AppendEntriesFromCommaSeparatedList: Append to sets
 * - AppendUniqueEntriesFromCommaSeparatedList: Append unique to arrays
 * - Reverse: Reverse array views
 * - InitArray: Initialize arrays with size
 * - ShiftArrayToSmallest: Rotate array to smallest element
 * - ArrayOfIndices: Generate index arrays
 *
 * Test naming convention: PCGEx.Unit.Helpers.ArrayHelpers.<FunctionName>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// GetStringArrayFromCommaSeparatedList Tests
// =============================================================================

/**
 * Test basic comma-separated string parsing
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersCommaSeparatedBasicTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.CommaSeparated.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersCommaSeparatedBasicTest::RunTest(const FString& Parameters)
{
	// Simple list
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("a,b,c"));
		TestEqual(TEXT("Three items count"), Result.Num(), 3);
		TestEqual(TEXT("First item"), Result[0], TEXT("a"));
		TestEqual(TEXT("Second item"), Result[1], TEXT("b"));
		TestEqual(TEXT("Third item"), Result[2], TEXT("c"));
	}

	// Single item
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("single"));
		TestEqual(TEXT("Single item count"), Result.Num(), 1);
		TestEqual(TEXT("Single item value"), Result[0], TEXT("single"));
	}

	// Empty string
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT(""));
		TestTrue(TEXT("Empty string results in empty or single empty array"),
		         Result.Num() == 0 || (Result.Num() == 1 && Result[0].IsEmpty()));
	}

	return true;
}

/**
 * Test comma-separated parsing with whitespace
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersCommaSeparatedWhitespaceTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.CommaSeparated.Whitespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersCommaSeparatedWhitespaceTest::RunTest(const FString& Parameters)
{
	// Items with surrounding whitespace
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("  a  ,  b  ,  c  "));
		TestEqual(TEXT("Three items with whitespace"), Result.Num(), 3);
		// Check if whitespace is trimmed
		TestTrue(TEXT("Items are trimmed or preserved"),
		         Result[0] == TEXT("a") || Result[0] == TEXT("  a  "));
	}

	// Items with internal whitespace
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("hello world,foo bar"));
		TestEqual(TEXT("Two items with spaces"), Result.Num(), 2);
	}

	return true;
}

/**
 * Test comma-separated parsing edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersCommaSeparatedEdgeCasesTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.CommaSeparated.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersCommaSeparatedEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Trailing comma
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("a,b,c,"));
		TestTrue(TEXT("Trailing comma handled"), Result.Num() >= 3);
	}

	// Leading comma
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT(",a,b,c"));
		TestTrue(TEXT("Leading comma handled"), Result.Num() >= 3);
	}

	// Multiple consecutive commas
	{
		TArray<FString> Result = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(TEXT("a,,b,,,c"));
		TestTrue(TEXT("Multiple commas handled"), Result.Num() >= 3);
	}

	return true;
}

// =============================================================================
// AppendEntriesFromCommaSeparatedList Tests
// =============================================================================

/**
 * Test appending to sets
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersAppendToSetTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.AppendToSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersAppendToSetTest::RunTest(const FString& Parameters)
{
	// Append to empty set
	{
		TSet<FString> Set;
		PCGExArrayHelpers::AppendEntriesFromCommaSeparatedList(TEXT("a,b,c"), Set);
		TestEqual(TEXT("Set has three entries"), Set.Num(), 3);
		TestTrue(TEXT("Set contains 'a'"), Set.Contains(TEXT("a")));
		TestTrue(TEXT("Set contains 'b'"), Set.Contains(TEXT("b")));
		TestTrue(TEXT("Set contains 'c'"), Set.Contains(TEXT("c")));
	}

	// Append to existing set with duplicates
	{
		TSet<FString> Set;
		Set.Add(TEXT("a"));
		Set.Add(TEXT("x"));
		PCGExArrayHelpers::AppendEntriesFromCommaSeparatedList(TEXT("a,b,c"), Set);
		TestEqual(TEXT("Set has four entries (no duplicate 'a')"), Set.Num(), 4);
		TestTrue(TEXT("Set contains 'x'"), Set.Contains(TEXT("x")));
	}

	return true;
}

// =============================================================================
// AppendUniqueEntriesFromCommaSeparatedList Tests
// =============================================================================

/**
 * Test appending unique entries to arrays
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersAppendUniqueTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.AppendUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersAppendUniqueTest::RunTest(const FString& Parameters)
{
	// Append to empty array
	{
		TArray<FString> Array;
		PCGExArrayHelpers::AppendUniqueEntriesFromCommaSeparatedList(TEXT("a,b,c"), Array);
		TestEqual(TEXT("Array has three entries"), Array.Num(), 3);
	}

	// Append with duplicates in source
	{
		TArray<FString> Array;
		PCGExArrayHelpers::AppendUniqueEntriesFromCommaSeparatedList(TEXT("a,b,a,c,b"), Array);
		TestEqual(TEXT("Array has three unique entries"), Array.Num(), 3);
	}

	// Append to existing array
	{
		TArray<FString> Array;
		Array.Add(TEXT("a"));
		Array.Add(TEXT("x"));
		PCGExArrayHelpers::AppendUniqueEntriesFromCommaSeparatedList(TEXT("a,b,c"), Array);
		TestEqual(TEXT("Array has four unique entries"), Array.Num(), 4);
	}

	return true;
}

// =============================================================================
// Reverse Tests
// =============================================================================

/**
 * Test array view reversal
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersReverseTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.Reverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersReverseTest::RunTest(const FString& Parameters)
{
	// Reverse integers
	{
		TArray<int32> Array = {1, 2, 3, 4, 5};
		PCGExArrayHelpers::Reverse<int32>(TArrayView<int32>(Array));
		TestEqual(TEXT("First becomes last"), Array[0], 5);
		TestEqual(TEXT("Second becomes fourth"), Array[1], 4);
		TestEqual(TEXT("Middle stays middle"), Array[2], 3);
		TestEqual(TEXT("Fourth becomes second"), Array[3], 2);
		TestEqual(TEXT("Last becomes first"), Array[4], 1);
	}

	// Reverse even count
	{
		TArray<int32> Array = {1, 2, 3, 4};
		PCGExArrayHelpers::Reverse<int32>(TArrayView<int32>(Array));
		TestEqual(TEXT("Even: first becomes last"), Array[0], 4);
		TestEqual(TEXT("Even: last becomes first"), Array[3], 1);
	}

	// Reverse single element
	{
		TArray<int32> Array = {42};
		PCGExArrayHelpers::Reverse<int32>(TArrayView<int32>(Array));
		TestEqual(TEXT("Single element unchanged"), Array[0], 42);
	}

	// Reverse empty
	{
		TArray<int32> Array;
		PCGExArrayHelpers::Reverse<int32>(TArrayView<int32>(Array));
		TestEqual(TEXT("Empty array stays empty"), Array.Num(), 0);
	}

	// Reverse strings
	{
		TArray<FString> Array = {TEXT("a"), TEXT("b"), TEXT("c")};
		PCGExArrayHelpers::Reverse<FString>(TArrayView<FString>(Array));
		TestEqual(TEXT("String first"), Array[0], TEXT("c"));
		TestEqual(TEXT("String last"), Array[2], TEXT("a"));
	}

	return true;
}

// =============================================================================
// InitArray Tests
// =============================================================================

/**
 * Test array initialization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersInitArrayTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.InitArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersInitArrayTest::RunTest(const FString& Parameters)
{
	// Initialize int array
	{
		TArray<int32> Array;
		PCGExArrayHelpers::InitArray(Array, 10);
		TestEqual(TEXT("Int array has 10 elements"), Array.Num(), 10);
	}

	// Initialize float array
	{
		TArray<float> Array;
		PCGExArrayHelpers::InitArray(Array, 5);
		TestEqual(TEXT("Float array has 5 elements"), Array.Num(), 5);
	}

	// Initialize with zero
	{
		TArray<int32> Array;
		PCGExArrayHelpers::InitArray(Array, 0);
		TestEqual(TEXT("Zero-size array is empty"), Array.Num(), 0);
	}

	// Initialize shared ptr
	{
		TSharedPtr<TArray<int32>> Array;
		PCGExArrayHelpers::InitArray(Array, 5);
		TestNotNull(TEXT("Shared array created"), Array.Get());
		TestEqual(TEXT("Shared array has 5 elements"), Array->Num(), 5);
	}

	return true;
}

// =============================================================================
// ShiftArrayToSmallest Tests
// =============================================================================

/**
 * Test shifting array to smallest element
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersShiftToSmallestTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.ShiftToSmallest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersShiftToSmallestTest::RunTest(const FString& Parameters)
{
	// Already starts with smallest
	{
		TArray<int32> Array = {1, 2, 3, 4, 5};
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("Starts with smallest: first = 1"), Array[0], 1);
	}

	// Smallest in middle
	{
		TArray<int32> Array = {3, 4, 1, 2, 5};
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("Middle smallest: first = 1"), Array[0], 1);
		// After shift: {1, 2, 5, 3, 4}
		TestEqual(TEXT("Middle smallest: second = 2"), Array[1], 2);
	}

	// Smallest at end
	{
		TArray<int32> Array = {5, 4, 3, 2, 1};
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("End smallest: first = 1"), Array[0], 1);
	}

	// Single element
	{
		TArray<int32> Array = {42};
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("Single element unchanged"), Array[0], 42);
	}

	// Empty array
	{
		TArray<int32> Array;
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("Empty array stays empty"), Array.Num(), 0);
	}

	// With duplicates
	{
		TArray<int32> Array = {3, 1, 1, 2};
		PCGExArrayHelpers::ShiftArrayToSmallest(Array);
		TestEqual(TEXT("Duplicates: first = 1"), Array[0], 1);
	}

	return true;
}

// =============================================================================
// ArrayOfIndices Tests
// =============================================================================

/**
 * Test generating arrays of indices
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersArrayOfIndicesTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.ArrayOfIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersArrayOfIndicesTest::RunTest(const FString& Parameters)
{
	// Basic index array
	{
		TArray<int32> Indices;
		PCGExArrayHelpers::ArrayOfIndices(Indices, 5);
		TestEqual(TEXT("Has 5 elements"), Indices.Num(), 5);
		TestEqual(TEXT("First index = 0"), Indices[0], 0);
		TestEqual(TEXT("Last index = 4"), Indices[4], 4);
	}

	// With offset
	{
		TArray<int32> Indices;
		PCGExArrayHelpers::ArrayOfIndices(Indices, 3, 10);
		TestEqual(TEXT("Has 3 elements"), Indices.Num(), 3);
		TestEqual(TEXT("First index = 10"), Indices[0], 10);
		TestEqual(TEXT("Last index = 12"), Indices[2], 12);
	}

	// Zero count
	{
		TArray<int32> Indices;
		PCGExArrayHelpers::ArrayOfIndices(Indices, 0);
		TestEqual(TEXT("Zero count gives empty array"), Indices.Num(), 0);
	}

	return true;
}

/**
 * Test generating indices from mask (int8)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersArrayOfIndicesMaskInt8Test,
	"PCGEx.Unit.Helpers.ArrayHelpers.ArrayOfIndices.MaskInt8",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersArrayOfIndicesMaskInt8Test::RunTest(const FString& Parameters)
{
	// Basic mask
	{
		TArray<int8> Mask = {1, 0, 1, 0, 1};
		TArray<int32> Indices;
		int32 Count = PCGExArrayHelpers::ArrayOfIndices(Indices, TArrayView<const int8>(Mask), 0, false);
		TestEqual(TEXT("Count of masked indices"), Count, 3);
		TestTrue(TEXT("Contains index 0"), Indices.Contains(0));
		TestTrue(TEXT("Contains index 2"), Indices.Contains(2));
		TestTrue(TEXT("Contains index 4"), Indices.Contains(4));
		TestFalse(TEXT("Does not contain index 1"), Indices.Contains(1));
	}

	// Inverted mask
	{
		TArray<int8> Mask = {1, 0, 1, 0, 1};
		TArray<int32> Indices;
		int32 Count = PCGExArrayHelpers::ArrayOfIndices(Indices, TArrayView<const int8>(Mask), 0, true);
		TestEqual(TEXT("Inverted count"), Count, 2);
		TestTrue(TEXT("Inverted contains index 1"), Indices.Contains(1));
		TestTrue(TEXT("Inverted contains index 3"), Indices.Contains(3));
	}

	// With offset
	{
		TArray<int8> Mask = {1, 1, 0};
		TArray<int32> Indices;
		int32 Count = PCGExArrayHelpers::ArrayOfIndices(Indices, TArrayView<const int8>(Mask), 100, false);
		TestEqual(TEXT("Offset count"), Count, 2);
		TestTrue(TEXT("Offset contains 100"), Indices.Contains(100));
		TestTrue(TEXT("Offset contains 101"), Indices.Contains(101));
	}

	return true;
}

/**
 * Test generating indices from TBitArray mask
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersArrayOfIndicesMaskBitArrayTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.ArrayOfIndices.MaskBitArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersArrayOfIndicesMaskBitArrayTest::RunTest(const FString& Parameters)
{
	// Basic bit mask
	{
		TBitArray<> Mask;
		Mask.Init(false, 5);
		Mask[0] = true;
		Mask[2] = true;
		Mask[4] = true;

		TArray<int32> Indices;
		int32 Count = PCGExArrayHelpers::ArrayOfIndices(Indices, Mask, 0, false);
		TestEqual(TEXT("BitArray count"), Count, 3);
		TestTrue(TEXT("BitArray contains 0"), Indices.Contains(0));
		TestTrue(TEXT("BitArray contains 2"), Indices.Contains(2));
		TestTrue(TEXT("BitArray contains 4"), Indices.Contains(4));
	}

	// Inverted bit mask
	{
		TBitArray<> Mask;
		Mask.Init(true, 5);
		Mask[1] = false;
		Mask[3] = false;

		TArray<int32> Indices;
		int32 Count = PCGExArrayHelpers::ArrayOfIndices(Indices, Mask, 0, true);
		// Inverted: false becomes true, so indices 1 and 3
		TestEqual(TEXT("BitArray inverted count"), Count, 2);
	}

	return true;
}

// =============================================================================
// ReorderArray Tests
// =============================================================================

/**
 * Test array reordering by index array
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExArrayHelpersReorderArrayTest,
	"PCGEx.Unit.Helpers.ArrayHelpers.ReorderArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExArrayHelpersReorderArrayTest::RunTest(const FString& Parameters)
{
	// Simple reorder
	{
		TArray<int32> Array = {10, 20, 30, 40, 50};
		TArray<int32> Order = {4, 3, 2, 1, 0}; // Reverse order
		PCGExArrayHelpers::ReorderArray(Array, Order);
		TestEqual(TEXT("Reordered first = 50"), Array[0], 50);
		TestEqual(TEXT("Reordered last = 10"), Array[4], 10);
	}

	// Identity reorder (no change)
	{
		TArray<int32> Array = {1, 2, 3, 4, 5};
		TArray<int32> Order = {0, 1, 2, 3, 4};
		PCGExArrayHelpers::ReorderArray(Array, Order);
		TestEqual(TEXT("Identity: first = 1"), Array[0], 1);
		TestEqual(TEXT("Identity: last = 5"), Array[4], 5);
	}

	// Cycle reorder
	{
		TArray<int32> Array = {10, 20, 30};
		TArray<int32> Order = {1, 2, 0}; // Shift left
		PCGExArrayHelpers::ReorderArray(Array, Order);
		// Position 0 gets value from position 1 (20)
		// Position 1 gets value from position 2 (30)
		// Position 2 gets value from position 0 (10)
		TestEqual(TEXT("Cycle: first = 20"), Array[0], 20);
		TestEqual(TEXT("Cycle: second = 30"), Array[1], 30);
		TestEqual(TEXT("Cycle: third = 10"), Array[2], 10);
	}

	return true;
}
