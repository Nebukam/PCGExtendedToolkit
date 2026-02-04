// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMathDistances Unit Tests
 *
 * Tests distance calculation utilities:
 * - GetDistances factory function
 * - GetNoneDistances function
 * - IDistances interface (FVector-based methods)
 * - Euclidean, Manhattan, Chebyshev distance types
 *
 * Note: FPoint-based methods require PCG data context and are not tested here.
 *
 * Test naming convention: PCGEx.Unit.MathDistances.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExMathDistances.h"
#include "Math/PCGExMath.h"

using namespace PCGExMath;

// =============================================================================
// Factory Tests
// =============================================================================

/**
 * Test GetDistances factory returns valid instances
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesFactoryTest,
	"PCGEx.Unit.MathDistances.Factory.GetDistances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesFactoryTest::RunTest(const FString& Parameters)
{
	// Test all distance type combinations
	TArray<EPCGExDistanceType> Types = {
		EPCGExDistanceType::Euclidian,
		EPCGExDistanceType::Manhattan,
		EPCGExDistanceType::Chebyshev
	};

	TArray<EPCGExDistance> Modes = {
		EPCGExDistance::Center,
		EPCGExDistance::SphereBounds,
		EPCGExDistance::BoxBounds,
		EPCGExDistance::None
	};

	for (EPCGExDistanceType Type : Types)
	{
		for (EPCGExDistance Source : Modes)
		{
			for (EPCGExDistance Target : Modes)
			{
				const IDistances* Distances = GetDistances(Source, Target, false, Type);
				TestNotNull(FString::Printf(TEXT("GetDistances returns non-null for Type=%d, Source=%d, Target=%d"),
				                            static_cast<int>(Type), static_cast<int>(Source), static_cast<int>(Target)),
				            Distances);
			}
		}
	}

	return true;
}

/**
 * Test GetDistances with overlap flag
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesFactoryOverlapTest,
	"PCGEx.Unit.MathDistances.Factory.OverlapFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesFactoryOverlapTest::RunTest(const FString& Parameters)
{
	// Both overlap settings should return valid instances
	const IDistances* WithOverlap = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, true);
	const IDistances* WithoutOverlap = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false);

	TestNotNull(TEXT("With overlap returns non-null"), WithOverlap);
	TestNotNull(TEXT("Without overlap returns non-null"), WithoutOverlap);

	// They should have different bOverlapIsZero values
	TestTrue(TEXT("With overlap has bOverlapIsZero=true"), WithOverlap->bOverlapIsZero);
	TestFalse(TEXT("Without overlap has bOverlapIsZero=false"), WithoutOverlap->bOverlapIsZero);

	return true;
}

/**
 * Test GetNoneDistances function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesGetNoneTest,
	"PCGEx.Unit.MathDistances.Factory.GetNoneDistances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesGetNoneTest::RunTest(const FString& Parameters)
{
	const IDistances* None = GetNoneDistances();

	TestNotNull(TEXT("GetNoneDistances returns non-null"), None);

	// Should be equivalent to GetDistances(None, None)
	const IDistances* Equivalent = GetDistances(EPCGExDistance::None, EPCGExDistance::None);
	TestEqual(TEXT("GetNoneDistances equals GetDistances(None, None)"), None, Equivalent);

	return true;
}

// =============================================================================
// Euclidean Distance Tests
// =============================================================================

/**
 * Test Euclidean distance calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesEuclideanTest,
	"PCGEx.Unit.MathDistances.Euclidean.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesEuclideanTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Euclidian);

	FVector A(0, 0, 0);
	FVector B(100, 0, 0);

	// Distance along X axis
	double Dist = Distances->GetDist(A, B);
	TestTrue(TEXT("Distance from origin to (100,0,0) is 100"),
	         FMath::IsNearlyEqual(Dist, 100.0, 0.01));

	double DistSq = Distances->GetDistSquared(A, B);
	TestTrue(TEXT("Distance squared is 10000"),
	         FMath::IsNearlyEqual(DistSq, 10000.0, 0.01));

	// 3D distance
	FVector C(30, 40, 0); // 3-4-5 triangle scaled by 10
	Dist = Distances->GetDist(A, C);
	TestTrue(TEXT("Distance to (30,40,0) is 50"),
	         FMath::IsNearlyEqual(Dist, 50.0, 0.01));

	return true;
}

/**
 * Test Euclidean distance edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesEuclideanEdgeCasesTest,
	"PCGEx.Unit.MathDistances.Euclidean.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesEuclideanEdgeCasesTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Euclidian);

	// Same point
	FVector A(50, 50, 50);
	double Dist = Distances->GetDist(A, A);
	TestTrue(TEXT("Distance to same point is 0"),
	         FMath::IsNearlyEqual(Dist, 0.0, 0.0001));

	// Very small distance
	FVector B(50.001, 50, 50);
	Dist = Distances->GetDist(A, B);
	TestTrue(TEXT("Small distance is positive"),
	         Dist > 0.0 && Dist < 0.01);

	// Large distance
	FVector C(10000, 10000, 10000);
	Dist = Distances->GetDist(A, C);
	TestTrue(TEXT("Large distance calculated correctly"),
	         Dist > 17000.0); // sqrt(3 * 9950^2) ≈ 17234

	return true;
}

// =============================================================================
// Manhattan Distance Tests
// =============================================================================

/**
 * Test Manhattan distance calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesManhattanTest,
	"PCGEx.Unit.MathDistances.Manhattan.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesManhattanTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Manhattan);

	FVector A(0, 0, 0);
	FVector B(30, 40, 50);

	// Manhattan = |30| + |40| + |50| = 120
	double Dist = Distances->GetDist(A, B);
	TestTrue(TEXT("Manhattan distance is sum of absolute differences"),
	         FMath::IsNearlyEqual(Dist, 120.0, 0.01));

	// Squared should be 120^2 = 14400
	double DistSq = Distances->GetDistSquared(A, B);
	TestTrue(TEXT("Manhattan squared is 14400"),
	         FMath::IsNearlyEqual(DistSq, 14400.0, 0.01));

	return true;
}

/**
 * Test Manhattan distance with negative coordinates
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesManhattanNegativeTest,
	"PCGEx.Unit.MathDistances.Manhattan.Negative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesManhattanNegativeTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Manhattan);

	FVector A(-50, -50, -50);
	FVector B(50, 50, 50);

	// Manhattan = |100| + |100| + |100| = 300
	double Dist = Distances->GetDist(A, B);
	TestTrue(TEXT("Manhattan with negative coords is 300"),
	         FMath::IsNearlyEqual(Dist, 300.0, 0.01));

	return true;
}

// =============================================================================
// Chebyshev Distance Tests
// =============================================================================

/**
 * Test Chebyshev distance calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesChebyshevTest,
	"PCGEx.Unit.MathDistances.Chebyshev.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesChebyshevTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Chebyshev);

	FVector A(0, 0, 0);
	FVector B(30, 40, 50);

	// Chebyshev = max(|30|, |40|, |50|) = 50
	double Dist = Distances->GetDist(A, B);
	TestTrue(TEXT("Chebyshev distance is max absolute difference"),
	         FMath::IsNearlyEqual(Dist, 50.0, 0.01));

	// Squared should be 50^2 = 2500
	double DistSq = Distances->GetDistSquared(A, B);
	TestTrue(TEXT("Chebyshev squared is 2500"),
	         FMath::IsNearlyEqual(DistSq, 2500.0, 0.01));

	return true;
}

/**
 * Test Chebyshev distance with different max axes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesChebyshevAxesTest,
	"PCGEx.Unit.MathDistances.Chebyshev.Axes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesChebyshevAxesTest::RunTest(const FString& Parameters)
{
	const IDistances* Distances = GetDistances(
		EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Chebyshev);

	FVector Origin(0, 0, 0);

	// Max on X
	FVector MaxX(100, 50, 25);
	TestTrue(TEXT("Chebyshev with max X is 100"),
	         FMath::IsNearlyEqual(Distances->GetDist(Origin, MaxX), 100.0, 0.01));

	// Max on Y
	FVector MaxY(25, 100, 50);
	TestTrue(TEXT("Chebyshev with max Y is 100"),
	         FMath::IsNearlyEqual(Distances->GetDist(Origin, MaxY), 100.0, 0.01));

	// Max on Z
	FVector MaxZ(50, 25, 100);
	TestTrue(TEXT("Chebyshev with max Z is 100"),
	         FMath::IsNearlyEqual(Distances->GetDist(Origin, MaxZ), 100.0, 0.01));

	return true;
}

// =============================================================================
// Distance Comparison Tests
// =============================================================================

/**
 * Test that different distance types give different results for same points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesComparisonTest,
	"PCGEx.Unit.MathDistances.Comparison.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesComparisonTest::RunTest(const FString& Parameters)
{
	const IDistances* Euclidean = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Euclidian);
	const IDistances* Manhattan = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Manhattan);
	const IDistances* Chebyshev = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Chebyshev);

	FVector A(0, 0, 0);
	FVector B(30, 40, 0); // 2D for simpler verification

	double DistEuc = Euclidean->GetDist(A, B);  // sqrt(900+1600) = 50
	double DistMan = Manhattan->GetDist(A, B); // 30 + 40 = 70
	double DistCheb = Chebyshev->GetDist(A, B); // max(30, 40) = 40

	TestTrue(TEXT("Euclidean distance is 50"),
	         FMath::IsNearlyEqual(DistEuc, 50.0, 0.01));
	TestTrue(TEXT("Manhattan distance is 70"),
	         FMath::IsNearlyEqual(DistMan, 70.0, 0.01));
	TestTrue(TEXT("Chebyshev distance is 40"),
	         FMath::IsNearlyEqual(DistCheb, 40.0, 0.01));

	// Verify ordering: Chebyshev <= Euclidean <= Manhattan
	TestTrue(TEXT("Chebyshev <= Euclidean"), DistCheb <= DistEuc);
	TestTrue(TEXT("Euclidean <= Manhattan"), DistEuc <= DistMan);

	return true;
}

/**
 * Test distance on axis-aligned points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistancesAxisAlignedTest,
	"PCGEx.Unit.MathDistances.Comparison.AxisAligned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistancesAxisAlignedTest::RunTest(const FString& Parameters)
{
	const IDistances* Euclidean = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Euclidian);
	const IDistances* Manhattan = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Manhattan);
	const IDistances* Chebyshev = GetDistances(EPCGExDistance::Center, EPCGExDistance::Center, false, EPCGExDistanceType::Chebyshev);

	FVector A(0, 0, 0);
	FVector B(100, 0, 0); // Axis-aligned

	// All metrics should give the same result for axis-aligned points
	double DistEuc = Euclidean->GetDist(A, B);
	double DistMan = Manhattan->GetDist(A, B);
	double DistCheb = Chebyshev->GetDist(A, B);

	TestTrue(TEXT("All metrics equal 100 for axis-aligned"),
	         FMath::IsNearlyEqual(DistEuc, 100.0, 0.01) &&
	         FMath::IsNearlyEqual(DistMan, 100.0, 0.01) &&
	         FMath::IsNearlyEqual(DistCheb, 100.0, 0.01));

	return true;
}

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExDistance enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistanceEnumTest,
	"PCGEx.Unit.MathDistances.Enum.EPCGExDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistanceEnumTest::RunTest(const FString& Parameters)
{
	// Verify enum values for stability
	TestEqual(TEXT("Center = 0"),
	          static_cast<uint8>(EPCGExDistance::Center), static_cast<uint8>(0));
	TestEqual(TEXT("SphereBounds = 1"),
	          static_cast<uint8>(EPCGExDistance::SphereBounds), static_cast<uint8>(1));
	TestEqual(TEXT("BoxBounds = 2"),
	          static_cast<uint8>(EPCGExDistance::BoxBounds), static_cast<uint8>(2));
	TestEqual(TEXT("None = 3"),
	          static_cast<uint8>(EPCGExDistance::None), static_cast<uint8>(3));

	return true;
}

/**
 * Test EPCGExDistanceType enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDistanceTypeEnumTest,
	"PCGEx.Unit.MathDistances.Enum.EPCGExDistanceType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDistanceTypeEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Euclidian = 0"),
	          static_cast<uint8>(EPCGExDistanceType::Euclidian), static_cast<uint8>(0));
	TestEqual(TEXT("Manhattan = 1"),
	          static_cast<uint8>(EPCGExDistanceType::Manhattan), static_cast<uint8>(1));
	TestEqual(TEXT("Chebyshev = 2"),
	          static_cast<uint8>(EPCGExDistanceType::Chebyshev), static_cast<uint8>(2));

	return true;
}
