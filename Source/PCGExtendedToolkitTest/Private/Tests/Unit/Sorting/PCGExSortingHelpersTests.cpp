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

//////////////////////////////////////////////////////////////////////////
// Morton Hash Tests (PCGEx::MH64 - used in GraphBuilder for deterministic sorting)
//////////////////////////////////////////////////////////////////////////

#pragma region MortonHash

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashBasicTest,
	"PCGEx.Unit.Sorting.MortonHash.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashBasicTest::RunTest(const FString& Parameters)
{
	// Same position should always produce same hash
	const FVector Pos1(100.0, 200.0, 300.0);
	const uint64 Hash1A = PCGEx::MH64(Pos1);
	const uint64 Hash1B = PCGEx::MH64(Pos1);

	TestEqual(TEXT("Same position produces same hash"), Hash1A, Hash1B);

	// Different positions should produce different hashes
	const FVector Pos2(100.0, 200.0, 301.0);
	const uint64 Hash2 = PCGEx::MH64(Pos2);

	TestNotEqual(TEXT("Different positions produce different hashes"), Hash1A, Hash2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashNegativeTest,
	"PCGEx.Unit.Sorting.MortonHash.NegativeCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashNegativeTest::RunTest(const FString& Parameters)
{
	// Test with negative coordinates
	const FVector PosNeg(-100.0, -200.0, -300.0);
	const uint64 HashNegA = PCGEx::MH64(PosNeg);
	const uint64 HashNegB = PCGEx::MH64(PosNeg);

	TestEqual(TEXT("Negative coordinates produce consistent hash"), HashNegA, HashNegB);

	// Mixed positive/negative
	const FVector PosMixed(-100.0, 200.0, -300.0);
	const uint64 HashMixedA = PCGEx::MH64(PosMixed);
	const uint64 HashMixedB = PCGEx::MH64(PosMixed);

	TestEqual(TEXT("Mixed coordinates produce consistent hash"), HashMixedA, HashMixedB);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashDeterminismTest,
	"PCGEx.Unit.Sorting.MortonHash.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	// Create test positions
	TArray<FVector> Positions;
	Positions.Add(FVector(100.0, 200.0, 300.0));
	Positions.Add(FVector(50.0, 150.0, 250.0));
	Positions.Add(FVector(200.0, 100.0, 400.0));
	Positions.Add(FVector(75.0, 175.0, 275.0));
	Positions.Add(FVector(150.0, 50.0, 350.0));

	// Sort in original order
	TArray<FIndexKey> Keys1;
	for (int32 i = 0; i < Positions.Num(); i++)
	{
		Keys1.Add({i, PCGEx::MH64(Positions[i])});
	}
	RadixSort(Keys1);

	// Sort in reverse order
	TArray<FIndexKey> Keys2;
	for (int32 i = Positions.Num() - 1; i >= 0; i--)
	{
		Keys2.Add({i, PCGEx::MH64(Positions[i])});
	}
	RadixSort(Keys2);

	// Sort in shuffled order
	TArray<FIndexKey> Keys3;
	TArray<int32> ShuffledIndices = {2, 0, 4, 1, 3};
	for (int32 i : ShuffledIndices)
	{
		Keys3.Add({i, PCGEx::MH64(Positions[i])});
	}
	RadixSort(Keys3);

	// All three should produce the same final order (same sequence of Index values)
	TestEqual(TEXT("Same result regardless of input order - count"), Keys1.Num(), Keys2.Num());
	TestEqual(TEXT("Same result regardless of input order - count"), Keys1.Num(), Keys3.Num());

	for (int32 i = 0; i < Keys1.Num(); i++)
	{
		TestEqual(*FString::Printf(TEXT("Keys1 vs Keys2 at %d"), i), Keys1[i].Index, Keys2[i].Index);
		TestEqual(*FString::Printf(TEXT("Keys1 vs Keys3 at %d"), i), Keys1[i].Index, Keys3[i].Index);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashStabilityTest,
	"PCGEx.Unit.Sorting.MortonHash.StabilityWithDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashStabilityTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	// Test with duplicate positions (same hash)
	const FVector SamePos(100.0, 200.0, 300.0);
	const FVector DiffPos(50.0, 100.0, 150.0);

	// Multiple items with same position
	TArray<FIndexKey> Keys;
	Keys.Add({0, PCGEx::MH64(SamePos)});
	Keys.Add({1, PCGEx::MH64(DiffPos)});
	Keys.Add({2, PCGEx::MH64(SamePos)});
	Keys.Add({3, PCGEx::MH64(SamePos)});
	Keys.Add({4, PCGEx::MH64(DiffPos)});

	RadixSort(Keys);

	// Verify all keys with same position are grouped together
	uint64 LastKey = 0;
	bool bGroupedCorrectly = true;
	for (int32 i = 0; i < Keys.Num(); i++)
	{
		if (i > 0 && Keys[i].Key < LastKey)
		{
			bGroupedCorrectly = false;
			break;
		}
		LastKey = Keys[i].Key;
	}
	TestTrue(TEXT("Keys are sorted in ascending order"), bGroupedCorrectly);

	// RadixSort is stable, so for duplicate keys the relative order should be preserved
	// Items with DiffPos hash should maintain relative order (1 before 4)
	// Items with SamePos hash should maintain relative order (0 before 2 before 3)

	// Find indices of items with same hash
	TArray<int32> SamePosIndices;
	TArray<int32> DiffPosIndices;
	const uint64 SamePosHash = PCGEx::MH64(SamePos);
	const uint64 DiffPosHash = PCGEx::MH64(DiffPos);

	for (const FIndexKey& Key : Keys)
	{
		if (Key.Key == SamePosHash)
		{
			SamePosIndices.Add(Key.Index);
		}
		else if (Key.Key == DiffPosHash)
		{
			DiffPosIndices.Add(Key.Index);
		}
	}

	// Check stable order for SamePos items (0, 2, 3)
	if (SamePosIndices.Num() >= 3)
	{
		TestTrue(TEXT("Stable order for duplicate keys - first"), SamePosIndices[0] < SamePosIndices[1]);
		TestTrue(TEXT("Stable order for duplicate keys - second"), SamePosIndices[1] < SamePosIndices[2]);
	}

	// Check stable order for DiffPos items (1, 4)
	if (DiffPosIndices.Num() >= 2)
	{
		TestTrue(TEXT("Stable order for duplicate keys - diff"), DiffPosIndices[0] < DiffPosIndices[1]);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashRepeatedSortTest,
	"PCGEx.Unit.Sorting.MortonHash.RepeatedSort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashRepeatedSortTest::RunTest(const FString& Parameters)
{
	using namespace PCGExSortingHelpers;
	using PCGEx::FIndexKey;

	// Create test positions
	TArray<FVector> Positions;
	Positions.Add(FVector(100.0, 200.0, 300.0));
	Positions.Add(FVector(50.0, 150.0, 250.0));
	Positions.Add(FVector(200.0, 100.0, 400.0));

	// Sort multiple times and verify same result
	TArray<TArray<int32>> Results;

	for (int32 Run = 0; Run < 10; Run++)
	{
		TArray<FIndexKey> Keys;
		for (int32 i = 0; i < Positions.Num(); i++)
		{
			Keys.Add({i, PCGEx::MH64(Positions[i])});
		}
		RadixSort(Keys);

		TArray<int32> Indices;
		for (const FIndexKey& Key : Keys)
		{
			Indices.Add(Key.Index);
		}
		Results.Add(Indices);
	}

	// All runs should produce identical results
	for (int32 Run = 1; Run < Results.Num(); Run++)
	{
		for (int32 i = 0; i < Results[0].Num(); i++)
		{
			TestEqual(*FString::Printf(TEXT("Run %d matches Run 0 at index %d"), Run, i),
				Results[Run][i], Results[0][i]);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSortingMortonHashCollisionTest,
	"PCGEx.Unit.Sorting.MortonHash.HashCollisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSortingMortonHashCollisionTest::RunTest(const FString& Parameters)
{
	// Test for potential hash collisions with close positions
	// The Morton hash uses: (X*1000 << 42) ^ (Y*1000 << 21) ^ (Z*1000)

	// Very close positions should have different hashes (within 0.001 unit)
	const FVector Pos1(100.0, 200.0, 300.0);
	const FVector Pos2(100.001, 200.0, 300.0);
	const FVector Pos3(100.0, 200.001, 300.0);
	const FVector Pos4(100.0, 200.0, 300.001);

	const uint64 Hash1 = PCGEx::MH64(Pos1);
	const uint64 Hash2 = PCGEx::MH64(Pos2);
	const uint64 Hash3 = PCGEx::MH64(Pos3);
	const uint64 Hash4 = PCGEx::MH64(Pos4);

	// Due to the *1000 multiplier, 0.001 difference = 1 unit difference in hash input
	TestNotEqual(TEXT("Small X difference produces different hash"), Hash1, Hash2);
	TestNotEqual(TEXT("Small Y difference produces different hash"), Hash1, Hash3);
	TestNotEqual(TEXT("Small Z difference produces different hash"), Hash1, Hash4);

	// Test positions that are too close (within floating point tolerance of * 1000)
	const FVector Pos5(100.0, 200.0, 300.0);
	const FVector Pos6(100.0000001, 200.0, 300.0); // 0.0000001 * 1000 = 0.0001, truncates to same int

	const uint64 Hash5 = PCGEx::MH64(Pos5);
	const uint64 Hash6 = PCGEx::MH64(Pos6);

	// These might collide due to int truncation - document this behavior
	AddInfo(FString::Printf(TEXT("Hash5: %llu, Hash6: %llu, Same: %s"),
		Hash5, Hash6, Hash5 == Hash6 ? TEXT("Yes") : TEXT("No")));

	return true;
}

#pragma endregion
