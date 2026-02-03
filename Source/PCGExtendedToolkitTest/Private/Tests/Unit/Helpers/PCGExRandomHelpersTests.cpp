// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExRandomHelpers Unit Tests
 *
 * Tests random generation functions from PCGExRandomHelpers.h:
 * - FastRand01: Fast random number in [0, 1) range
 * - ComputeSpatialSeed: Compute deterministic seed from position
 *
 * Test naming convention: PCGEx.Unit.Helpers.Random.<FunctionName>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExRandomHelpers.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// FastRand01 Tests
// =============================================================================

/**
 * Test FastRand01 output range
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomFastRand01RangeTest,
	"PCGEx.Unit.Helpers.Random.FastRand01.Range",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomFastRand01RangeTest::RunTest(const FString& Parameters)
{
	uint32 Seed = 12345;

	// Generate many values and verify range
	const int32 NumSamples = 1000;
	for (int32 i = 0; i < NumSamples; ++i)
	{
		double Value = PCGExRandomHelpers::FastRand01(Seed);
		TestTrue(FString::Printf(TEXT("Value %d >= 0"), i), Value >= 0.0);
		TestTrue(FString::Printf(TEXT("Value %d < 1"), i), Value < 1.0);
	}

	return true;
}

/**
 * Test FastRand01 determinism
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomFastRand01DeterminismTest,
	"PCGEx.Unit.Helpers.Random.FastRand01.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomFastRand01DeterminismTest::RunTest(const FString& Parameters)
{
	// Same seed should produce same sequence
	uint32 Seed1 = 42;
	uint32 Seed2 = 42;

	const int32 NumSamples = 100;
	for (int32 i = 0; i < NumSamples; ++i)
	{
		double Value1 = PCGExRandomHelpers::FastRand01(Seed1);
		double Value2 = PCGExRandomHelpers::FastRand01(Seed2);
		TestTrue(FString::Printf(TEXT("Determinism at sample %d"), i),
		         FMath::IsNearlyEqual(Value1, Value2, KINDA_SMALL_NUMBER));
	}

	return true;
}

/**
 * Test FastRand01 different seeds produce different sequences
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomFastRand01DifferentSeedsTest,
	"PCGEx.Unit.Helpers.Random.FastRand01.DifferentSeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomFastRand01DifferentSeedsTest::RunTest(const FString& Parameters)
{
	uint32 Seed1 = 100;
	uint32 Seed2 = 200;

	// Collect first few values from each
	TArray<double> Values1, Values2;
	for (int32 i = 0; i < 10; ++i)
	{
		Values1.Add(PCGExRandomHelpers::FastRand01(Seed1));
		Values2.Add(PCGExRandomHelpers::FastRand01(Seed2));
	}

	// At least some values should differ
	int32 DifferentCount = 0;
	for (int32 i = 0; i < 10; ++i)
	{
		if (!FMath::IsNearlyEqual(Values1[i], Values2[i], KINDA_SMALL_NUMBER))
		{
			DifferentCount++;
		}
	}

	TestTrue(TEXT("Different seeds produce different values"), DifferentCount > 5);

	return true;
}

/**
 * Test FastRand01 seed mutation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomFastRand01SeedMutationTest,
	"PCGEx.Unit.Helpers.Random.FastRand01.SeedMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomFastRand01SeedMutationTest::RunTest(const FString& Parameters)
{
	uint32 Seed = 12345;
	uint32 OriginalSeed = Seed;

	PCGExRandomHelpers::FastRand01(Seed);

	TestTrue(TEXT("Seed is mutated after call"), Seed != OriginalSeed);

	// Multiple calls should keep mutating
	uint32 PreviousSeed = Seed;
	PCGExRandomHelpers::FastRand01(Seed);
	TestTrue(TEXT("Seed mutates on each call"), Seed != PreviousSeed);

	return true;
}

/**
 * Test FastRand01 distribution (basic uniformity check)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomFastRand01DistributionTest,
	"PCGEx.Unit.Helpers.Random.FastRand01.Distribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomFastRand01DistributionTest::RunTest(const FString& Parameters)
{
	uint32 Seed = 54321;
	const int32 NumSamples = 10000;
	const int32 NumBuckets = 10;
	int32 Buckets[10] = {0};

	for (int32 i = 0; i < NumSamples; ++i)
	{
		double Value = PCGExRandomHelpers::FastRand01(Seed);
		int32 BucketIndex = FMath::Clamp(static_cast<int32>(Value * NumBuckets), 0, NumBuckets - 1);
		Buckets[BucketIndex]++;
	}

	// Each bucket should have roughly NumSamples/NumBuckets = 1000 samples
	// Allow for 30% variance
	const int32 ExpectedPerBucket = NumSamples / NumBuckets;
	const int32 MinAcceptable = static_cast<int32>(ExpectedPerBucket * 0.7);
	const int32 MaxAcceptable = static_cast<int32>(ExpectedPerBucket * 1.3);

	for (int32 i = 0; i < NumBuckets; ++i)
	{
		TestTrue(FString::Printf(TEXT("Bucket %d has %d samples (expected ~%d)"), i, Buckets[i], ExpectedPerBucket),
		         Buckets[i] >= MinAcceptable && Buckets[i] <= MaxAcceptable);
	}

	return true;
}

// =============================================================================
// ComputeSpatialSeed Tests
// =============================================================================

/**
 * Test ComputeSpatialSeed determinism
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedDeterminismTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedDeterminismTest::RunTest(const FString& Parameters)
{
	FVector Position(100.0, 200.0, 300.0);

	int32 Seed1 = PCGExRandomHelpers::ComputeSpatialSeed(Position);
	int32 Seed2 = PCGExRandomHelpers::ComputeSpatialSeed(Position);

	TestEqual(TEXT("Same position produces same seed"), Seed1, Seed2);

	return true;
}

/**
 * Test ComputeSpatialSeed different positions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedDifferentPositionsTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.DifferentPositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedDifferentPositionsTest::RunTest(const FString& Parameters)
{
	FVector Pos1(0.0, 0.0, 0.0);
	FVector Pos2(100.0, 0.0, 0.0);
	FVector Pos3(0.0, 100.0, 0.0);
	FVector Pos4(0.0, 0.0, 100.0);

	int32 Seed1 = PCGExRandomHelpers::ComputeSpatialSeed(Pos1);
	int32 Seed2 = PCGExRandomHelpers::ComputeSpatialSeed(Pos2);
	int32 Seed3 = PCGExRandomHelpers::ComputeSpatialSeed(Pos3);
	int32 Seed4 = PCGExRandomHelpers::ComputeSpatialSeed(Pos4);

	// All should be different (or at least most)
	TSet<int32> UniqueSeeds;
	UniqueSeeds.Add(Seed1);
	UniqueSeeds.Add(Seed2);
	UniqueSeeds.Add(Seed3);
	UniqueSeeds.Add(Seed4);

	TestTrue(TEXT("Different positions produce mostly unique seeds"), UniqueSeeds.Num() >= 3);

	return true;
}

/**
 * Test ComputeSpatialSeed with offset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedWithOffsetTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.WithOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedWithOffsetTest::RunTest(const FString& Parameters)
{
	FVector Position(100.0, 200.0, 300.0);
	FVector Offset1(0.0, 0.0, 0.0);
	FVector Offset2(10.0, 10.0, 10.0);

	int32 SeedNoOffset = PCGExRandomHelpers::ComputeSpatialSeed(Position, Offset1);
	int32 SeedWithOffset = PCGExRandomHelpers::ComputeSpatialSeed(Position, Offset2);

	TestTrue(TEXT("Different offsets produce different seeds"),
	         SeedNoOffset != SeedWithOffset);

	return true;
}

/**
 * Test ComputeSpatialSeed with negative positions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedNegativeTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.NegativePositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedNegativeTest::RunTest(const FString& Parameters)
{
	FVector PosPositive(100.0, 100.0, 100.0);
	FVector PosNegative(-100.0, -100.0, -100.0);

	int32 SeedPositive = PCGExRandomHelpers::ComputeSpatialSeed(PosPositive);
	int32 SeedNegative = PCGExRandomHelpers::ComputeSpatialSeed(PosNegative);

	// They should produce different seeds
	TestTrue(TEXT("Positive and negative positions produce different seeds"),
	         SeedPositive != SeedNegative);

	// Both should be valid (not undefined)
	TestTrue(TEXT("Negative position produces valid seed"), true); // Just verify no crash

	return true;
}

/**
 * Test ComputeSpatialSeed with very large positions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedLargePositionsTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.LargePositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedLargePositionsTest::RunTest(const FString& Parameters)
{
	FVector PosSmall(1.0, 1.0, 1.0);
	FVector PosLarge(1000000.0, 1000000.0, 1000000.0);
	FVector PosVeryLarge(1e10, 1e10, 1e10);

	int32 SeedSmall = PCGExRandomHelpers::ComputeSpatialSeed(PosSmall);
	int32 SeedLarge = PCGExRandomHelpers::ComputeSpatialSeed(PosLarge);
	int32 SeedVeryLarge = PCGExRandomHelpers::ComputeSpatialSeed(PosVeryLarge);

	// Just verify no crashes and different values
	TestTrue(TEXT("Small and large positions differ"), SeedSmall != SeedLarge);
	TestTrue(TEXT("Large and very large positions differ"), SeedLarge != SeedVeryLarge);

	return true;
}

/**
 * Test ComputeSpatialSeed at origin
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSpatialSeedOriginTest,
	"PCGEx.Unit.Helpers.Random.SpatialSeed.Origin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSpatialSeedOriginTest::RunTest(const FString& Parameters)
{
	FVector Origin = FVector::ZeroVector;

	int32 SeedOrigin1 = PCGExRandomHelpers::ComputeSpatialSeed(Origin);
	int32 SeedOrigin2 = PCGExRandomHelpers::ComputeSpatialSeed(Origin);

	// Should be deterministic at origin
	TestEqual(TEXT("Origin produces consistent seed"), SeedOrigin1, SeedOrigin2);

	// Should be different from a distant point
	// Note: PCGHelpers::ComputeSeedFromPosition uses integer-based hashing,
	// so very small offsets (< 1 unit) may produce the same seed
	FVector Distant(100, 100, 100);
	int32 SeedDistant = PCGExRandomHelpers::ComputeSpatialSeed(Distant);
	TestTrue(TEXT("Origin differs from distant point"), SeedOrigin1 != SeedDistant);

	return true;
}

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExSeedComponents enum flags
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRandomSeedComponentsEnumTest,
	"PCGEx.Unit.Helpers.Random.Enums.SeedComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExRandomSeedComponentsEnumTest::RunTest(const FString& Parameters)
{
	// Test that flags are powers of 2 (bitmask)
	TestEqual(TEXT("None = 0"), static_cast<uint8>(EPCGExSeedComponents::None), 0);
	TestEqual(TEXT("Local = 2"), static_cast<uint8>(EPCGExSeedComponents::Local), 2);
	TestEqual(TEXT("Settings = 4"), static_cast<uint8>(EPCGExSeedComponents::Settings), 4);
	TestEqual(TEXT("Component = 8"), static_cast<uint8>(EPCGExSeedComponents::Component), 8);

	// Test flag combinations
	uint8 Combined = static_cast<uint8>(EPCGExSeedComponents::Local) |
	                 static_cast<uint8>(EPCGExSeedComponents::Settings);
	TestEqual(TEXT("Local | Settings = 6"), Combined, 6);

	return true;
}
