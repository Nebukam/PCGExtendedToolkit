// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Core/PCGExAssetCollection.h"
#include "PCGExCollectionsCommon.h"

/**
 * Tests for PCGExAssetCollection cache system
 * Covers: FCategory, FMicroCache, FCache, EPCGExIndexPickMode
 */

//////////////////////////////////////////////////////////////////////////
// FCategory Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FCategory

namespace PCGExCollectionCacheTestHelpers
{
	/**
	 * Helper struct to simulate collection entries for testing
	 */
	struct FTestEntry : public FPCGExAssetCollectionEntry
	{
		FTestEntry() = default;
		explicit FTestEntry(int32 InWeight, FName InCategory = NAME_None)
		{
			Weight = InWeight;
			Category = InCategory;
		}
	};

	/**
	 * Creates a category with test entries
	 */
	TSharedPtr<PCGExAssetCollection::FCategory> CreateTestCategory(const TArray<FTestEntry>& Entries)
	{
		auto Category = MakeShared<PCGExAssetCollection::FCategory>(NAME_None);
		Category->Reserve(Entries.Num());

		for (int32 i = 0; i < Entries.Num(); i++)
		{
			Category->Indices.Add(i);
			Category->Weights.Add(Entries[i].Weight + 1); // +1 as in implementation
		}

		// Compile manually (simplified version without entry pointers)
		Category->Shrink();

		const int32 NumEntries = Category->Indices.Num();
		Category->Order.SetNumUninitialized(NumEntries);
		for (int32 i = 0; i < NumEntries; i++)
		{
			Category->Order[i] = i;
		}

		// Sort Order by weights (ascending)
		Category->Order.Sort([&Category](int32 A, int32 B) { return Category->Weights[A] < Category->Weights[B]; });
		// Sort Weights (ascending)
		Category->Weights.Sort([](int32 A, int32 B) { return A < B; });

		// Build cumulative weights
		Category->WeightSum = 0;
		for (int32 i = 0; i < NumEntries; i++)
		{
			Category->WeightSum += Category->Weights[i];
			Category->Weights[i] = static_cast<int32>(Category->WeightSum);
		}

		return Category;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryEmptyTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryEmptyTest::RunTest(const FString& Parameters)
{
	auto Category = MakeShared<PCGExAssetCollection::FCategory>(NAME_None);

	TestTrue(TEXT("Empty category IsEmpty"), Category->IsEmpty());
	TestEqual(TEXT("Empty category Num is 0"), Category->Num(), 0);
	TestEqual(TEXT("GetPickAscending on empty returns -1"), Category->GetPickAscending(0), -1);
	TestEqual(TEXT("GetPickDescending on empty returns -1"), Category->GetPickDescending(0), -1);
	TestEqual(TEXT("GetPickRandom on empty returns -1"), Category->GetPickRandom(12345), -1);
	TestEqual(TEXT("GetPickRandomWeighted on empty returns -1"), Category->GetPickRandomWeighted(12345), -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategorySingleEntryTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.SingleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategorySingleEntryTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(10));

	auto Category = CreateTestCategory(Entries);

	TestFalse(TEXT("Single entry category not empty"), Category->IsEmpty());
	TestEqual(TEXT("Single entry category Num is 1"), Category->Num(), 1);

	// All pick methods should return index 0 for valid input
	TestEqual(TEXT("GetPickAscending(0) returns 0"), Category->GetPickAscending(0), 0);
	TestEqual(TEXT("GetPickDescending(0) returns 0"), Category->GetPickDescending(0), 0);
	TestEqual(TEXT("GetPickWeightAscending(0) returns 0"), Category->GetPickWeightAscending(0), 0);
	TestEqual(TEXT("GetPickWeightDescending(0) returns 0"), Category->GetPickWeightDescending(0), 0);

	// Invalid indices should return -1
	TestEqual(TEXT("GetPickAscending(1) returns -1"), Category->GetPickAscending(1), -1);
	TestEqual(TEXT("GetPickDescending(-1) returns -1"), Category->GetPickDescending(-1), -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryMultipleEntriesAscendingTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.Ascending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryMultipleEntriesAscendingTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(10)); // Index 0
	Entries.Add(FTestEntry(20)); // Index 1
	Entries.Add(FTestEntry(30)); // Index 2
	Entries.Add(FTestEntry(40)); // Index 3
	Entries.Add(FTestEntry(50)); // Index 4

	auto Category = CreateTestCategory(Entries);

	TestEqual(TEXT("Category Num is 5"), Category->Num(), 5);

	// Ascending should return entries in order
	TestEqual(TEXT("GetPickAscending(0) returns 0"), Category->GetPickAscending(0), 0);
	TestEqual(TEXT("GetPickAscending(1) returns 1"), Category->GetPickAscending(1), 1);
	TestEqual(TEXT("GetPickAscending(2) returns 2"), Category->GetPickAscending(2), 2);
	TestEqual(TEXT("GetPickAscending(3) returns 3"), Category->GetPickAscending(3), 3);
	TestEqual(TEXT("GetPickAscending(4) returns 4"), Category->GetPickAscending(4), 4);
	TestEqual(TEXT("GetPickAscending(5) returns -1"), Category->GetPickAscending(5), -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryMultipleEntriesDescendingTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.Descending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryMultipleEntriesDescendingTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(10)); // Index 0
	Entries.Add(FTestEntry(20)); // Index 1
	Entries.Add(FTestEntry(30)); // Index 2
	Entries.Add(FTestEntry(40)); // Index 3
	Entries.Add(FTestEntry(50)); // Index 4

	auto Category = CreateTestCategory(Entries);

	// Descending should return entries in reverse order
	TestEqual(TEXT("GetPickDescending(0) returns 4"), Category->GetPickDescending(0), 4);
	TestEqual(TEXT("GetPickDescending(1) returns 3"), Category->GetPickDescending(1), 3);
	TestEqual(TEXT("GetPickDescending(2) returns 2"), Category->GetPickDescending(2), 2);
	TestEqual(TEXT("GetPickDescending(3) returns 1"), Category->GetPickDescending(3), 1);
	TestEqual(TEXT("GetPickDescending(4) returns 0"), Category->GetPickDescending(4), 0);
	TestEqual(TEXT("GetPickDescending(5) returns -1"), Category->GetPickDescending(5), -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightOrderTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightOrderTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Create entries with non-sequential weights
	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(50)); // Index 0, highest weight
	Entries.Add(FTestEntry(10)); // Index 1, lowest weight
	Entries.Add(FTestEntry(30)); // Index 2, middle weight
	Entries.Add(FTestEntry(20)); // Index 3, second lowest
	Entries.Add(FTestEntry(40)); // Index 4, second highest

	auto Category = CreateTestCategory(Entries);

	// WeightAscending: sorted by weight ascending (lowest first)
	// Weight order (ascending): 10, 20, 30, 40, 50 -> indices 1, 3, 2, 4, 0
	TestEqual(TEXT("GetPickWeightAscending(0) returns index 1 (weight 10)"), Category->GetPickWeightAscending(0), 1);
	TestEqual(TEXT("GetPickWeightAscending(1) returns index 3 (weight 20)"), Category->GetPickWeightAscending(1), 3);
	TestEqual(TEXT("GetPickWeightAscending(2) returns index 2 (weight 30)"), Category->GetPickWeightAscending(2), 2);
	TestEqual(TEXT("GetPickWeightAscending(3) returns index 4 (weight 40)"), Category->GetPickWeightAscending(3), 4);
	TestEqual(TEXT("GetPickWeightAscending(4) returns index 0 (weight 50)"), Category->GetPickWeightAscending(4), 0);

	// WeightDescending: sorted by weight descending (highest first)
	TestEqual(TEXT("GetPickWeightDescending(0) returns index 0 (weight 50)"), Category->GetPickWeightDescending(0), 0);
	TestEqual(TEXT("GetPickWeightDescending(1) returns index 4 (weight 40)"), Category->GetPickWeightDescending(1), 4);
	TestEqual(TEXT("GetPickWeightDescending(2) returns index 2 (weight 30)"), Category->GetPickWeightDescending(2), 2);
	TestEqual(TEXT("GetPickWeightDescending(3) returns index 3 (weight 20)"), Category->GetPickWeightDescending(3), 3);
	TestEqual(TEXT("GetPickWeightDescending(4) returns index 1 (weight 10)"), Category->GetPickWeightDescending(4), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryRandomDeterministicTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.RandomDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryRandomDeterministicTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	TArray<FTestEntry> Entries;
	for (int32 i = 0; i < 10; i++)
	{
		Entries.Add(FTestEntry(1)); // Equal weights
	}

	auto Category = CreateTestCategory(Entries);

	// Same seed should produce same result
	const int32 Seed = 42;
	const int32 FirstPick = Category->GetPickRandom(Seed);
	const int32 SecondPick = Category->GetPickRandom(Seed);

	TestEqual(TEXT("Same seed produces same result"), FirstPick, SecondPick);
	TestTrue(TEXT("Random pick is valid index"), FirstPick >= 0 && FirstPick < 10);

	// Different seeds should (usually) produce different results
	const int32 DifferentSeedPick = Category->GetPickRandom(999);
	// Note: We don't assert inequality since random could theoretically produce same value

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomDistributionTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandomDistribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomDistributionTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Create entries with very different weights
	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(1));    // Index 0, very low weight
	Entries.Add(FTestEntry(1000)); // Index 1, very high weight

	auto Category = CreateTestCategory(Entries);

	// Run many iterations and count picks
	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 1000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	// Index 1 (high weight) should be picked significantly more than index 0
	const int32 LowWeightPicks = PickCounts.FindOrAdd(0);
	const int32 HighWeightPicks = PickCounts.FindOrAdd(1);

	// High weight entry should have much more picks
	TestTrue(TEXT("High weight entry picked more often"), HighWeightPicks > LowWeightPicks * 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomSubtleVariationTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandom.SubtleVariation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomSubtleVariationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Test subtle weight differences: 10, 20, 30 (2x and 3x ratios)
	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(10)); // Index 0, weight 10
	Entries.Add(FTestEntry(20)); // Index 1, weight 20 (2x of 0)
	Entries.Add(FTestEntry(30)); // Index 2, weight 30 (3x of 0)

	auto Category = CreateTestCategory(Entries);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 10000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	const int32 Count0 = PickCounts.FindOrAdd(0);
	const int32 Count1 = PickCounts.FindOrAdd(1);
	const int32 Count2 = PickCounts.FindOrAdd(2);

	// With weights 10:20:30 (after +1: 11:21:31), ratios should be roughly 11:21:31
	// Allow 30% tolerance for statistical variation
	const double Ratio1to0 = static_cast<double>(Count1) / FMath::Max(Count0, 1);
	const double Ratio2to0 = static_cast<double>(Count2) / FMath::Max(Count0, 1);

	// Expected: 21/11 ≈ 1.9, 31/11 ≈ 2.8
	TestTrue(TEXT("Weight 20 picked ~2x more than weight 10"), Ratio1to0 > 1.3 && Ratio1to0 < 2.8);
	TestTrue(TEXT("Weight 30 picked ~3x more than weight 10"), Ratio2to0 > 1.8 && Ratio2to0 < 4.0);
	TestTrue(TEXT("Weight 30 picked more than weight 20"), Count2 > Count1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomManyEntriesTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandom.ManyEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomManyEntriesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Create 20 entries with linearly increasing weights
	TArray<FTestEntry> Entries;
	for (int32 i = 0; i < 20; i++)
	{
		Entries.Add(FTestEntry((i + 1) * 5)); // Weights: 5, 10, 15, ... 100
	}

	auto Category = CreateTestCategory(Entries);

	TestEqual(TEXT("Category has 20 entries"), Category->Num(), 20);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 20000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	// Verify all entries were picked at least once
	int32 PickedEntries = 0;
	for (int32 i = 0; i < 20; i++)
	{
		if (PickCounts.Contains(i) && PickCounts[i] > 0)
		{
			PickedEntries++;
		}
	}
	TestTrue(TEXT("All 20 entries were picked at least once"), PickedEntries == 20);

	// Higher weight entries should be picked more often on average
	// Compare first 5 entries (low weight) vs last 5 entries (high weight)
	int32 LowWeightTotal = 0;
	int32 HighWeightTotal = 0;
	for (int32 i = 0; i < 5; i++)
	{
		LowWeightTotal += PickCounts.FindOrAdd(i);
		HighWeightTotal += PickCounts.FindOrAdd(15 + i);
	}

	// Last 5 entries have weights 80-100 vs first 5 with weights 5-25
	// Ratio should be roughly (80+85+90+95+100)/(5+10+15+20+25) = 450/75 = 6x
	TestTrue(TEXT("High weight entries picked significantly more than low weight"),
		HighWeightTotal > LowWeightTotal * 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomEqualWeightsTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandom.EqualWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomEqualWeightsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Create 5 entries with equal weights
	TArray<FTestEntry> Entries;
	for (int32 i = 0; i < 5; i++)
	{
		Entries.Add(FTestEntry(100)); // All equal weight
	}

	auto Category = CreateTestCategory(Entries);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 10000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	// With equal weights, distribution should be roughly uniform
	// Expected: ~2000 picks each, allow 40% tolerance
	const int32 ExpectedPerEntry = NumIterations / 5;
	const int32 MinExpected = static_cast<int32>(ExpectedPerEntry * 0.6);
	const int32 MaxExpected = static_cast<int32>(ExpectedPerEntry * 1.4);

	for (int32 i = 0; i < 5; i++)
	{
		const int32 Count = PickCounts.FindOrAdd(i);
		TestTrue(*FString::Printf(TEXT("Entry %d count (%d) within expected range [%d, %d]"), i, Count, MinExpected, MaxExpected),
			Count >= MinExpected && Count <= MaxExpected);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomExtremeRatiosTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandom.ExtremeRatios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomExtremeRatiosTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Create entries with extreme weight ratios: 1, 100, 10000
	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(1));     // Index 0, tiny weight
	Entries.Add(FTestEntry(100));   // Index 1, medium weight
	Entries.Add(FTestEntry(10000)); // Index 2, huge weight

	auto Category = CreateTestCategory(Entries);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 50000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	const int32 TinyCount = PickCounts.FindOrAdd(0);
	const int32 MediumCount = PickCounts.FindOrAdd(1);
	const int32 HugeCount = PickCounts.FindOrAdd(2);

	// Huge weight should dominate
	TestTrue(TEXT("Huge weight dominates picks"), HugeCount > MediumCount * 10);
	TestTrue(TEXT("Medium weight > tiny weight"), MediumCount > TinyCount * 10);

	// Tiny weight entry should still get picked occasionally
	TestTrue(TEXT("Tiny weight entry still picked occasionally"), TinyCount > 0);

	// Huge weight should be vast majority
	const double HugeRatio = static_cast<double>(HugeCount) / NumIterations;
	TestTrue(TEXT("Huge weight is >90% of picks"), HugeRatio > 0.90);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryWeightedRandomProportionalityTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.WeightedRandom.Proportionality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryWeightedRandomProportionalityTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Test that distribution matches weight proportions
	// Weights: 100, 200, 300, 400 -> After +1: 101, 201, 301, 401
	// Total: 1004, proportions: ~10%, ~20%, ~30%, ~40%
	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(100)); // ~10%
	Entries.Add(FTestEntry(200)); // ~20%
	Entries.Add(FTestEntry(300)); // ~30%
	Entries.Add(FTestEntry(400)); // ~40%

	auto Category = CreateTestCategory(Entries);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 50000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandomWeighted(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	// Calculate actual proportions
	TArray<double> ActualProportions;
	for (int32 i = 0; i < 4; i++)
	{
		ActualProportions.Add(static_cast<double>(PickCounts.FindOrAdd(i)) / NumIterations);
	}

	// Expected proportions (with +1 adjustment): 101/1004, 201/1004, 301/1004, 401/1004
	const double Total = 101.0 + 201.0 + 301.0 + 401.0;
	TArray<double> ExpectedProportions = {101.0 / Total, 201.0 / Total, 301.0 / Total, 401.0 / Total};

	// Verify proportions are within 25% of expected (relative error)
	for (int32 i = 0; i < 4; i++)
	{
		const double RelativeError = FMath::Abs(ActualProportions[i] - ExpectedProportions[i]) / ExpectedProportions[i];
		TestTrue(*FString::Printf(TEXT("Entry %d proportion (%.3f) within 25%% of expected (%.3f)"),
			i, ActualProportions[i], ExpectedProportions[i]),
			RelativeError < 0.25);
	}

	// Verify ordering is correct
	TestTrue(TEXT("Entry 3 picked most"), PickCounts[3] > PickCounts[2]);
	TestTrue(TEXT("Entry 2 picked more than entry 1"), PickCounts[2] > PickCounts[1]);
	TestTrue(TEXT("Entry 1 picked more than entry 0"), PickCounts[1] > PickCounts[0]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryUniformRandomDistributionTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.UniformRandom.Distribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryUniformRandomDistributionTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	// Test GetPickRandom (non-weighted) produces uniform distribution
	TArray<FTestEntry> Entries;
	for (int32 i = 0; i < 10; i++)
	{
		// Use varying weights to prove GetPickRandom ignores them
		Entries.Add(FTestEntry((i + 1) * 100));
	}

	auto Category = CreateTestCategory(Entries);

	TMap<int32, int32> PickCounts;
	const int32 NumIterations = 20000;

	for (int32 i = 0; i < NumIterations; i++)
	{
		const int32 Pick = Category->GetPickRandom(i);
		PickCounts.FindOrAdd(Pick)++;
	}

	// Uniform distribution: each entry should get ~10% = 2000 picks
	// Allow 40% tolerance for statistical variation
	const int32 ExpectedPerEntry = NumIterations / 10;
	const int32 MinExpected = static_cast<int32>(ExpectedPerEntry * 0.6);
	const int32 MaxExpected = static_cast<int32>(ExpectedPerEntry * 1.4);

	int32 OutOfRangeCount = 0;
	for (int32 i = 0; i < 10; i++)
	{
		const int32 Count = PickCounts.FindOrAdd(i);
		if (Count < MinExpected || Count > MaxExpected)
		{
			OutOfRangeCount++;
		}
	}

	// Allow at most 2 entries to be slightly outside range due to randomness
	TestTrue(TEXT("Most entries within expected uniform range"), OutOfRangeCount <= 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCategoryGetPickByModeTest,
	"PCGEx.Unit.Collections.CollectionCache.Category.GetPickByMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCategoryGetPickByModeTest::RunTest(const FString& Parameters)
{
	using namespace PCGExCollectionCacheTestHelpers;

	TArray<FTestEntry> Entries;
	Entries.Add(FTestEntry(30)); // Index 0
	Entries.Add(FTestEntry(10)); // Index 1, lowest
	Entries.Add(FTestEntry(20)); // Index 2

	auto Category = CreateTestCategory(Entries);

	// Test GetPick with different modes
	TestEqual(TEXT("GetPick with Ascending mode"),
		Category->GetPick(0, EPCGExIndexPickMode::Ascending),
		Category->GetPickAscending(0));

	TestEqual(TEXT("GetPick with Descending mode"),
		Category->GetPick(0, EPCGExIndexPickMode::Descending),
		Category->GetPickDescending(0));

	TestEqual(TEXT("GetPick with WeightAscending mode"),
		Category->GetPick(0, EPCGExIndexPickMode::WeightAscending),
		Category->GetPickWeightAscending(0));

	TestEqual(TEXT("GetPick with WeightDescending mode"),
		Category->GetPick(0, EPCGExIndexPickMode::WeightDescending),
		Category->GetPickWeightDescending(0));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FMicroCache Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FMicroCache

namespace PCGExCollectionCacheTestHelpers
{
	/**
	 * Test implementation of FMicroCache for unit testing
	 */
	class FTestMicroCache : public PCGExAssetCollection::FMicroCache
	{
	public:
		void InitFromWeights(TConstArrayView<int32> InWeights)
		{
			BuildFromWeights(InWeights);
		}

		// Expose protected members for testing
		double GetWeightSum() const { return WeightSum; }
		const TArray<int32>& GetWeights() const { return Weights; }
		const TArray<int32>& GetOrder() const { return Order; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheEmptyTest,
	"PCGEx.Unit.Collections.CollectionCache.MicroCache.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheEmptyTest::RunTest(const FString& Parameters)
{
	PCGExCollectionCacheTestHelpers::FTestMicroCache MicroCache;

	TestTrue(TEXT("Empty MicroCache IsEmpty"), MicroCache.IsEmpty());
	TestEqual(TEXT("Empty MicroCache Num is 0"), MicroCache.Num(), 0);
	TestEqual(TEXT("GetPickAscending on empty returns -1"), MicroCache.GetPickAscending(0), -1);
	TestEqual(TEXT("GetPickDescending on empty returns -1"), MicroCache.GetPickDescending(0), -1);
	TestEqual(TEXT("GetPickRandom on empty returns -1"), MicroCache.GetPickRandom(12345), -1);
	TestEqual(TEXT("GetPickRandomWeighted on empty returns -1"), MicroCache.GetPickRandomWeighted(12345), -1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheSingleEntryTest,
	"PCGEx.Unit.Collections.CollectionCache.MicroCache.SingleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheSingleEntryTest::RunTest(const FString& Parameters)
{
	PCGExCollectionCacheTestHelpers::FTestMicroCache MicroCache;
	TArray<int32> Weights = {10};
	MicroCache.InitFromWeights(Weights);

	TestFalse(TEXT("Single entry MicroCache not empty"), MicroCache.IsEmpty());
	TestEqual(TEXT("Single entry MicroCache Num is 1"), MicroCache.Num(), 1);

	// All pick methods should return index 0 for valid input
	TestEqual(TEXT("GetPickAscending(0) returns 0"), MicroCache.GetPickAscending(0), 0);
	TestEqual(TEXT("GetPickDescending(0) returns 0"), MicroCache.GetPickDescending(0), 0);
	TestEqual(TEXT("GetPickWeightAscending(0) returns 0"), MicroCache.GetPickWeightAscending(0), 0);
	TestEqual(TEXT("GetPickWeightDescending(0) returns 0"), MicroCache.GetPickWeightDescending(0), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheMultipleEntriesTest,
	"PCGEx.Unit.Collections.CollectionCache.MicroCache.MultipleEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheMultipleEntriesTest::RunTest(const FString& Parameters)
{
	PCGExCollectionCacheTestHelpers::FTestMicroCache MicroCache;
	// Weights: index 0=50 (high), index 1=10 (low), index 2=30 (mid), index 3=20, index 4=40
	TArray<int32> Weights = {50, 10, 30, 20, 40};
	MicroCache.InitFromWeights(Weights);

	TestEqual(TEXT("MicroCache Num is 5"), MicroCache.Num(), 5);

	// Ascending: returns raw indices 0,1,2,3,4
	TestEqual(TEXT("GetPickAscending(0) returns 0"), MicroCache.GetPickAscending(0), 0);
	TestEqual(TEXT("GetPickAscending(4) returns 4"), MicroCache.GetPickAscending(4), 4);

	// Descending: returns indices in reverse (Num-1)-Index
	TestEqual(TEXT("GetPickDescending(0) returns 4"), MicroCache.GetPickDescending(0), 4);
	TestEqual(TEXT("GetPickDescending(4) returns 0"), MicroCache.GetPickDescending(4), 0);

	// Weight order should be sorted by weight
	// Original: 0=50, 1=10, 2=30, 3=20, 4=40
	// Sorted ascending by weight: 1(10), 3(20), 2(30), 4(40), 0(50)
	TestEqual(TEXT("GetPickWeightAscending(0) returns index 1 (lowest weight)"), MicroCache.GetPickWeightAscending(0), 1);
	TestEqual(TEXT("GetPickWeightAscending(4) returns index 0 (highest weight)"), MicroCache.GetPickWeightAscending(4), 0);

	TestEqual(TEXT("GetPickWeightDescending(0) returns index 0 (highest weight)"), MicroCache.GetPickWeightDescending(0), 0);
	TestEqual(TEXT("GetPickWeightDescending(4) returns index 1 (lowest weight)"), MicroCache.GetPickWeightDescending(4), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheWeightSumTest,
	"PCGEx.Unit.Collections.CollectionCache.MicroCache.WeightSum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheWeightSumTest::RunTest(const FString& Parameters)
{
	PCGExCollectionCacheTestHelpers::FTestMicroCache MicroCache;
	// Note: BuildFromWeights adds +1 to each weight
	TArray<int32> Weights = {10, 20, 30};
	MicroCache.InitFromWeights(Weights);

	// Weight sum should be (10+1) + (20+1) + (30+1) = 63
	TestTrue(TEXT("WeightSum is correct"), FMath::IsNearlyEqual(MicroCache.GetWeightSum(), 63.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMicroCacheGetPickByModeTest,
	"PCGEx.Unit.Collections.CollectionCache.MicroCache.GetPickByMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExMicroCacheGetPickByModeTest::RunTest(const FString& Parameters)
{
	PCGExCollectionCacheTestHelpers::FTestMicroCache MicroCache;
	TArray<int32> Weights = {30, 10, 20};
	MicroCache.InitFromWeights(Weights);

	// Test GetPick with different modes
	TestEqual(TEXT("GetPick with Ascending mode"),
		MicroCache.GetPick(0, EPCGExIndexPickMode::Ascending),
		MicroCache.GetPickAscending(0));

	TestEqual(TEXT("GetPick with Descending mode"),
		MicroCache.GetPick(0, EPCGExIndexPickMode::Descending),
		MicroCache.GetPickDescending(0));

	TestEqual(TEXT("GetPick with WeightAscending mode"),
		MicroCache.GetPick(0, EPCGExIndexPickMode::WeightAscending),
		MicroCache.GetPickWeightAscending(0));

	TestEqual(TEXT("GetPick with WeightDescending mode"),
		MicroCache.GetPick(0, EPCGExIndexPickMode::WeightDescending),
		MicroCache.GetPickWeightDescending(0));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FCache Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FCache

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCacheConstructorTest,
	"PCGEx.Unit.Collections.CollectionCache.Cache.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCacheConstructorTest::RunTest(const FString& Parameters)
{
	PCGExAssetCollection::FCache Cache;

	TestTrue(TEXT("Main category is initialized"), Cache.Main.IsValid());
	TestTrue(TEXT("Cache starts empty"), Cache.IsEmpty());
	TestEqual(TEXT("Categories map starts empty"), Cache.Categories.Num(), 0);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// EPCGExIndexPickMode Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EPCGExIndexPickMode

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExIndexPickModeEnumTest,
	"PCGEx.Unit.Collections.CollectionCache.IndexPickMode.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExIndexPickModeEnumTest::RunTest(const FString& Parameters)
{
	// Verify enum values
	TestEqual(TEXT("Ascending = 0"), static_cast<uint8>(EPCGExIndexPickMode::Ascending), 0);
	TestEqual(TEXT("Descending = 1"), static_cast<uint8>(EPCGExIndexPickMode::Descending), 1);
	TestEqual(TEXT("WeightAscending = 2"), static_cast<uint8>(EPCGExIndexPickMode::WeightAscending), 2);
	TestEqual(TEXT("WeightDescending = 3"), static_cast<uint8>(EPCGExIndexPickMode::WeightDescending), 3);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExEntryAccessResult Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExEntryAccessResult

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExEntryAccessResultDefaultTest,
	"PCGEx.Unit.Collections.CollectionCache.EntryAccessResult.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExEntryAccessResultDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExEntryAccessResult Result;

	TestFalse(TEXT("Default result is invalid"), Result.IsValid());
	TestFalse(TEXT("Default result operator bool is false"), static_cast<bool>(Result));
	TestTrue(TEXT("Entry is nullptr"), Result.Entry == nullptr);
	TestTrue(TEXT("Host is nullptr"), Result.Host == nullptr);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExEntryAccessResultValidTest,
	"PCGEx.Unit.Collections.CollectionCache.EntryAccessResult.Valid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExEntryAccessResultValidTest::RunTest(const FString& Parameters)
{
	FPCGExAssetCollectionEntry Entry;
	Entry.Weight = 10;

	FPCGExEntryAccessResult Result;
	Result.Entry = &Entry;
	// Note: Host is still nullptr, but IsValid only checks Entry

	TestTrue(TEXT("Result with Entry is valid"), Result.IsValid());
	TestTrue(TEXT("Result operator bool is true"), static_cast<bool>(Result));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FPCGExCollectionTypeSet Tests
//////////////////////////////////////////////////////////////////////////

#pragma region FPCGExCollectionTypeSet

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetDefaultTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet TypeSet;

	TestTrue(TEXT("Default type set is empty"), TypeSet.IsEmpty());
	TestEqual(TEXT("Default type set Num is 0"), TypeSet.Num(), 0);
	TestFalse(TEXT("Default type set doesn't contain Base"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Base));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetSingleTypeTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.SingleType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetSingleTypeTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet TypeSet(PCGExAssetCollection::TypeIds::Mesh);

	TestFalse(TEXT("Single type set is not empty"), TypeSet.IsEmpty());
	TestEqual(TEXT("Single type set Num is 1"), TypeSet.Num(), 1);
	TestTrue(TEXT("Contains Mesh"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestFalse(TEXT("Doesn't contain Actor"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Actor));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetInitializerListTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.InitializerList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetInitializerListTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet TypeSet({
		PCGExAssetCollection::TypeIds::Mesh,
		PCGExAssetCollection::TypeIds::Actor,
		PCGExAssetCollection::TypeIds::PCGDataAsset
	});

	TestEqual(TEXT("Type set Num is 3"), TypeSet.Num(), 3);
	TestTrue(TEXT("Contains Mesh"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestTrue(TEXT("Contains Actor"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Actor));
	TestTrue(TEXT("Contains PCGDataAsset"), TypeSet.Contains(PCGExAssetCollection::TypeIds::PCGDataAsset));
	TestFalse(TEXT("Doesn't contain Base"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Base));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetAddRemoveTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.AddRemove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetAddRemoveTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet TypeSet;

	// Add
	TypeSet.Add(PCGExAssetCollection::TypeIds::Mesh);
	TestTrue(TEXT("Contains Mesh after add"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestEqual(TEXT("Num is 1 after add"), TypeSet.Num(), 1);

	// Add another
	TypeSet.Add(PCGExAssetCollection::TypeIds::Actor);
	TestTrue(TEXT("Contains Actor after add"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Actor));
	TestEqual(TEXT("Num is 2 after second add"), TypeSet.Num(), 2);

	// Remove
	TypeSet.Remove(PCGExAssetCollection::TypeIds::Mesh);
	TestFalse(TEXT("Doesn't contain Mesh after remove"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestEqual(TEXT("Num is 1 after remove"), TypeSet.Num(), 1);
	TestTrue(TEXT("Still contains Actor"), TypeSet.Contains(PCGExAssetCollection::TypeIds::Actor));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetUnionTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.Union",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetUnionTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet SetA({PCGExAssetCollection::TypeIds::Mesh, PCGExAssetCollection::TypeIds::Actor});
	FPCGExCollectionTypeSet SetB({PCGExAssetCollection::TypeIds::Actor, PCGExAssetCollection::TypeIds::PCGDataAsset});

	FPCGExCollectionTypeSet UnionSet = SetA | SetB;

	TestEqual(TEXT("Union Num is 3"), UnionSet.Num(), 3);
	TestTrue(TEXT("Union contains Mesh"), UnionSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestTrue(TEXT("Union contains Actor"), UnionSet.Contains(PCGExAssetCollection::TypeIds::Actor));
	TestTrue(TEXT("Union contains PCGDataAsset"), UnionSet.Contains(PCGExAssetCollection::TypeIds::PCGDataAsset));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeSetIntersectionTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeSet.Intersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeSetIntersectionTest::RunTest(const FString& Parameters)
{
	FPCGExCollectionTypeSet SetA({PCGExAssetCollection::TypeIds::Mesh, PCGExAssetCollection::TypeIds::Actor});
	FPCGExCollectionTypeSet SetB({PCGExAssetCollection::TypeIds::Actor, PCGExAssetCollection::TypeIds::PCGDataAsset});

	FPCGExCollectionTypeSet IntersectSet = SetA & SetB;

	TestEqual(TEXT("Intersection Num is 1"), IntersectSet.Num(), 1);
	TestFalse(TEXT("Intersection doesn't contain Mesh"), IntersectSet.Contains(PCGExAssetCollection::TypeIds::Mesh));
	TestTrue(TEXT("Intersection contains Actor"), IntersectSet.Contains(PCGExAssetCollection::TypeIds::Actor));
	TestFalse(TEXT("Intersection doesn't contain PCGDataAsset"), IntersectSet.Contains(PCGExAssetCollection::TypeIds::PCGDataAsset));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// TypeIds Tests
//////////////////////////////////////////////////////////////////////////

#pragma region TypeIds

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollectionTypeIdsTest,
	"PCGEx.Unit.Collections.CollectionCache.TypeIds.Values",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExCollectionTypeIdsTest::RunTest(const FString& Parameters)
{
	// Verify standard type IDs are defined
	TestEqual(TEXT("None is NAME_None"), PCGExAssetCollection::TypeIds::None, NAME_None);
	TestEqual(TEXT("Base is 'Base'"), PCGExAssetCollection::TypeIds::Base.ToString(), TEXT("Base"));
	TestEqual(TEXT("Mesh is 'Mesh'"), PCGExAssetCollection::TypeIds::Mesh.ToString(), TEXT("Mesh"));
	TestEqual(TEXT("Actor is 'Actor'"), PCGExAssetCollection::TypeIds::Actor.ToString(), TEXT("Actor"));
	TestEqual(TEXT("PCGDataAsset is 'PCGDataAsset'"), PCGExAssetCollection::TypeIds::PCGDataAsset.ToString(), TEXT("PCGDataAsset"));

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// EPCGExDistribution Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EPCGExDistribution

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistributionEnumTest,
	"PCGEx.Unit.Collections.CollectionCache.Distribution.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExDistributionEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Index = 0"), static_cast<uint8>(EPCGExDistribution::Index), 0);
	TestEqual(TEXT("Random = 1"), static_cast<uint8>(EPCGExDistribution::Random), 1);
	TestEqual(TEXT("WeightedRandom = 2"), static_cast<uint8>(EPCGExDistribution::WeightedRandom), 2);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// EPCGExAssetTagInheritance Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EPCGExAssetTagInheritance

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetTagInheritanceEnumTest,
	"PCGEx.Unit.Collections.CollectionCache.TagInheritance.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetTagInheritanceEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("None = 0"), static_cast<uint8>(EPCGExAssetTagInheritance::None), 0);
	TestEqual(TEXT("Asset = 1 << 1"), static_cast<uint8>(EPCGExAssetTagInheritance::Asset), 1 << 1);
	TestEqual(TEXT("Hierarchy = 1 << 2"), static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy), 1 << 2);
	TestEqual(TEXT("Collection = 1 << 3"), static_cast<uint8>(EPCGExAssetTagInheritance::Collection), 1 << 3);
	TestEqual(TEXT("RootCollection = 1 << 4"), static_cast<uint8>(EPCGExAssetTagInheritance::RootCollection), 1 << 4);
	TestEqual(TEXT("RootAsset = 1 << 5"), static_cast<uint8>(EPCGExAssetTagInheritance::RootAsset), 1 << 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAssetTagInheritanceBitmaskTest,
	"PCGEx.Unit.Collections.CollectionCache.TagInheritance.Bitmask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAssetTagInheritanceBitmaskTest::RunTest(const FString& Parameters)
{
	// Test bitmask operations
	const uint8 Combined = static_cast<uint8>(EPCGExAssetTagInheritance::Asset) |
	                       static_cast<uint8>(EPCGExAssetTagInheritance::Collection);

	TestTrue(TEXT("Combined has Asset flag"),
		(Combined & static_cast<uint8>(EPCGExAssetTagInheritance::Asset)) != 0);
	TestTrue(TEXT("Combined has Collection flag"),
		(Combined & static_cast<uint8>(EPCGExAssetTagInheritance::Collection)) != 0);
	TestFalse(TEXT("Combined doesn't have Hierarchy flag"),
		(Combined & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy)) != 0);

	return true;
}

#pragma endregion
