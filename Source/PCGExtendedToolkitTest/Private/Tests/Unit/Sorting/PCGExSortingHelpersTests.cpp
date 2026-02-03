// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Sorting/PCGExSortingHelpers.h"

/**
 * Tests for PCGExSortingHelpers.h
 * Covers: FVectorKey, RadixSort
 */

//////////////////////////////////////////////////////////////////////////
// FVectorKey Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FVectorKey

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingFVectorKeyConstructorTest,
	"PCGEx.Unit.Sorting.SortingHelpers.FVectorKey.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingFVectorKeyConstructorTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;

	const FVector Vec(1.0, 2.0, 3.0);
	const FVectorKey Key(42, Vec);

	TestEqual(TEXT("Index is set correctly"), Key.Index, 42);
	TestTrue(TEXT("X component matches"), FMath::IsNearlyEqual(Key.X, 1.0));
	TestTrue(TEXT("Y component matches"), FMath::IsNearlyEqual(Key.Y, 2.0));
	TestTrue(TEXT("Z component matches"), FMath::IsNearlyEqual(Key.Z, 3.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingFVectorKeyComparisonTest,
	"PCGEx.Unit.Sorting.SortingHelpers.FVectorKey.Comparison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingFVectorKeyComparisonTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;

	// Primary sort by X
	const FVectorKey A(0, FVector(1.0, 5.0, 5.0));
	const FVectorKey B(1, FVector(2.0, 0.0, 0.0));
	TestTrue(TEXT("A < B when A.X < B.X"), A < B);
	TestFalse(TEXT("B not < A when A.X < B.X"), B < A);

	// Secondary sort by Y when X equal
	const FVectorKey C(2, FVector(1.0, 1.0, 5.0));
	const FVectorKey D(3, FVector(1.0, 2.0, 0.0));
	TestTrue(TEXT("C < D when X equal, C.Y < D.Y"), C < D);
	TestFalse(TEXT("D not < C when X equal, C.Y < D.Y"), D < C);

	// Tertiary sort by Z when X and Y equal
	const FVectorKey E(4, FVector(1.0, 1.0, 1.0));
	const FVectorKey F(5, FVector(1.0, 1.0, 2.0));
	TestTrue(TEXT("E < F when X,Y equal, E.Z < F.Z"), E < F);
	TestFalse(TEXT("F not < E when X,Y equal, E.Z < F.Z"), F < E);

	// Equal vectors
	const FVectorKey G(6, FVector(1.0, 1.0, 1.0));
	const FVectorKey H(7, FVector(1.0, 1.0, 1.0));
	TestFalse(TEXT("G not < H when equal"), G < H);
	TestFalse(TEXT("H not < G when equal"), H < G);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingFVectorKeySortArrayTest,
	"PCGEx.Unit.Sorting.SortingHelpers.FVectorKey.SortArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingFVectorKeySortArrayTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;

	TArray<FVectorKey> Keys;
	Keys.Add(FVectorKey(0, FVector(3.0, 0.0, 0.0)));
	Keys.Add(FVectorKey(1, FVector(1.0, 0.0, 0.0)));
	Keys.Add(FVectorKey(2, FVector(2.0, 0.0, 0.0)));
	Keys.Add(FVectorKey(3, FVector(1.0, 2.0, 0.0)));
	Keys.Add(FVectorKey(4, FVector(1.0, 1.0, 0.0)));

	Keys.Sort();

	// Should be sorted: (1,0,0), (1,1,0), (1,2,0), (2,0,0), (3,0,0)
	TestEqual(TEXT("First element index"), Keys[0].Index, 1);  // (1,0,0)
	TestEqual(TEXT("Second element index"), Keys[1].Index, 4); // (1,1,0)
	TestEqual(TEXT("Third element index"), Keys[2].Index, 3);  // (1,2,0)
	TestEqual(TEXT("Fourth element index"), Keys[3].Index, 2); // (2,0,0)
	TestEqual(TEXT("Fifth element index"), Keys[4].Index, 0);  // (3,0,0)

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// RadixSort Tests
//////////////////////////////////////////////////////////////////////////

#pragma region RadixSort

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortEmptyTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortEmptyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	RadixSort(Keys);

	TestEqual(TEXT("Empty array stays empty"), Keys.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortSingleTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.Single",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortSingleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	Keys.Add({42, 100});
	RadixSort(Keys);

	TestEqual(TEXT("Single element array unchanged"), Keys.Num(), 1);
	TestEqual(TEXT("Single element index unchanged"), Keys[0].Index, 42);
	TestEqual(TEXT("Single element key unchanged"), Keys[0].Key, 100ull);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortBasicTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortBasicTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	Keys.Add({0, 500});
	Keys.Add({1, 100});
	Keys.Add({2, 300});
	Keys.Add({3, 200});
	Keys.Add({4, 400});

	RadixSort(Keys);

	// Should be sorted by Key: 100, 200, 300, 400, 500
	TestEqual(TEXT("First key"), Keys[0].Key, 100ull);
	TestEqual(TEXT("First index"), Keys[0].Index, 1);

	TestEqual(TEXT("Second key"), Keys[1].Key, 200ull);
	TestEqual(TEXT("Second index"), Keys[1].Index, 3);

	TestEqual(TEXT("Third key"), Keys[2].Key, 300ull);
	TestEqual(TEXT("Third index"), Keys[2].Index, 2);

	TestEqual(TEXT("Fourth key"), Keys[3].Key, 400ull);
	TestEqual(TEXT("Fourth index"), Keys[3].Index, 4);

	TestEqual(TEXT("Fifth key"), Keys[4].Key, 500ull);
	TestEqual(TEXT("Fifth index"), Keys[4].Index, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortLargeKeysTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.LargeKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortLargeKeysTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	// Use large 64-bit keys
	Keys.Add({0, 0xFFFFFFFFFFFFFFFFull});
	Keys.Add({1, 0x0000000000000001ull});
	Keys.Add({2, 0x8000000000000000ull}); // High bit set
	Keys.Add({3, 0x0000000100000000ull}); // 32-bit boundary
	Keys.Add({4, 0x0000000000000000ull});

	RadixSort(Keys);

	// Should be sorted ascending by 64-bit key value
	TestEqual(TEXT("Smallest key is 0"), Keys[0].Key, 0x0000000000000000ull);
	TestEqual(TEXT("Smallest key index"), Keys[0].Index, 4);

	TestEqual(TEXT("Second smallest key"), Keys[1].Key, 0x0000000000000001ull);
	TestEqual(TEXT("Second smallest index"), Keys[1].Index, 1);

	TestEqual(TEXT("Third key (32-bit boundary)"), Keys[2].Key, 0x0000000100000000ull);
	TestEqual(TEXT("Third key index"), Keys[2].Index, 3);

	TestEqual(TEXT("Fourth key (high bit)"), Keys[3].Key, 0x8000000000000000ull);
	TestEqual(TEXT("Fourth key index"), Keys[3].Index, 2);

	TestEqual(TEXT("Largest key"), Keys[4].Key, 0xFFFFFFFFFFFFFFFFull);
	TestEqual(TEXT("Largest key index"), Keys[4].Index, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortDuplicateKeysTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.DuplicateKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortDuplicateKeysTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	Keys.Add({0, 100});
	Keys.Add({1, 100});
	Keys.Add({2, 50});
	Keys.Add({3, 100});
	Keys.Add({4, 50});

	RadixSort(Keys);

	// Should be sorted - all 50s first, then all 100s
	TestEqual(TEXT("First key is 50"), Keys[0].Key, 50ull);
	TestEqual(TEXT("Second key is 50"), Keys[1].Key, 50ull);
	TestEqual(TEXT("Third key is 100"), Keys[2].Key, 100ull);
	TestEqual(TEXT("Fourth key is 100"), Keys[3].Key, 100ull);
	TestEqual(TEXT("Fifth key is 100"), Keys[4].Key, 100ull);

	// Verify all elements present (stable sort preserves relative order for equal keys)
	TSet<int32> Indices;
	for (const auto& Key : Keys)
	{
		Indices.Add(Key.Index);
	}
	TestEqual(TEXT("All indices present"), Indices.Num(), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortAlreadySortedTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.AlreadySorted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortAlreadySortedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	Keys.Add({0, 10});
	Keys.Add({1, 20});
	Keys.Add({2, 30});
	Keys.Add({3, 40});
	Keys.Add({4, 50});

	RadixSort(Keys);

	// Should remain sorted
	for (int32 i = 0; i < 5; i++)
	{
		TestEqual(*FString::Printf(TEXT("Index %d"), i), Keys[i].Index, i);
		TestEqual(*FString::Printf(TEXT("Key %d"), i), Keys[i].Key, static_cast<uint64>((i + 1) * 10));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingRadixSortReverseSortedTest,
	"PCGEx.Unit.Sorting.SortingHelpers.RadixSort.ReverseSorted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingRadixSortReverseSortedTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	TArray<FIndexKey> Keys;
	Keys.Add({0, 50});
	Keys.Add({1, 40});
	Keys.Add({2, 30});
	Keys.Add({3, 20});
	Keys.Add({4, 10});

	RadixSort(Keys);

	// Should be sorted ascending
	TestEqual(TEXT("First"), Keys[0].Key, 10ull);
	TestEqual(TEXT("Second"), Keys[1].Key, 20ull);
	TestEqual(TEXT("Third"), Keys[2].Key, 30ull);
	TestEqual(TEXT("Fourth"), Keys[3].Key, 40ull);
	TestEqual(TEXT("Fifth"), Keys[4].Key, 50ull);

	return true;
}

#pragma endregion
