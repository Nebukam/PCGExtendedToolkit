// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Numeric Compare Filter Logic Unit Tests
 *
 * Tests numeric comparison filter logic without PCG context.
 * Simulates the FNumericCompareFilter::Test() behavior.
 *
 * Covered scenarios:
 * - All comparison operators (==, !=, >=, <=, >, <, ~=, !~=)
 * - Integer and floating point values
 * - Tolerance handling for nearly equal comparisons
 * - Edge cases (infinity, NaN-like behavior, etc.)
 *
 * Test naming convention: PCGEx.Unit.Filters.NumericCompareLogic.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Utils/PCGExCompare.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Numeric Compare Logic Simulation
// =============================================================================

/**
 * Simulates the numeric compare filter logic from FNumericCompareFilter.
 * This matches the actual Test() implementation.
 */
namespace NumericCompareLogic
{
	/**
	 * Test a numeric comparison.
	 * @param OperandA - First operand (typically from point attribute)
	 * @param Comparison - The comparison operator
	 * @param OperandB - Second operand (constant or from attribute)
	 * @param Tolerance - Tolerance for nearly equal comparisons
	 */
	bool Test(
		double OperandA,
		EPCGExComparison Comparison,
		double OperandB,
		double Tolerance = DBL_COMPARE_TOLERANCE)
	{
		return PCGExCompare::Compare(Comparison, OperandA, OperandB, Tolerance);
	}
}

// =============================================================================
// StrictlyEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareStrictlyEqualTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.StrictlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareStrictlyEqualTest::RunTest(const FString& Parameters)
{
	// Integer-like comparisons
	TestTrue(TEXT("5 == 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlyEqual, 5.0));
	TestFalse(TEXT("5 != 6"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlyEqual, 6.0));
	TestTrue(TEXT("0 == 0"), NumericCompareLogic::Test(0.0, EPCGExComparison::StrictlyEqual, 0.0));
	TestTrue(TEXT("-5 == -5"), NumericCompareLogic::Test(-5.0, EPCGExComparison::StrictlyEqual, -5.0));

	// Floating point
	TestTrue(TEXT("1.5 == 1.5"), NumericCompareLogic::Test(1.5, EPCGExComparison::StrictlyEqual, 1.5));

	// Beware: floating point precision issues
	// 0.1 + 0.2 may not exactly equal 0.3
	double Sum = 0.1 + 0.2;
	// This may fail due to floating point representation
	// Use NearlyEqual for such cases
	// TestTrue(TEXT("0.1+0.2 == 0.3"), NumericCompareLogic::Test(Sum, EPCGExComparison::StrictlyEqual, 0.3));

	return true;
}

// =============================================================================
// StrictlyNotEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareStrictlyNotEqualTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.StrictlyNotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareStrictlyNotEqualTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("5 != 6"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlyNotEqual, 6.0));
	TestFalse(TEXT("5 not != 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlyNotEqual, 5.0));
	TestTrue(TEXT("-1 != 1"), NumericCompareLogic::Test(-1.0, EPCGExComparison::StrictlyNotEqual, 1.0));
	TestTrue(TEXT("0 != 0.001"), NumericCompareLogic::Test(0.0, EPCGExComparison::StrictlyNotEqual, 0.001));

	return true;
}

// =============================================================================
// EqualOrGreater Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareEqualOrGreaterTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.EqualOrGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareEqualOrGreaterTest::RunTest(const FString& Parameters)
{
	// Equal case
	TestTrue(TEXT("5 >= 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::EqualOrGreater, 5.0));

	// Greater case
	TestTrue(TEXT("10 >= 5"), NumericCompareLogic::Test(10.0, EPCGExComparison::EqualOrGreater, 5.0));

	// Less case (fails)
	TestFalse(TEXT("3 not >= 5"), NumericCompareLogic::Test(3.0, EPCGExComparison::EqualOrGreater, 5.0));

	// Negative numbers
	TestTrue(TEXT("-3 >= -5"), NumericCompareLogic::Test(-3.0, EPCGExComparison::EqualOrGreater, -5.0));
	TestFalse(TEXT("-7 not >= -5"), NumericCompareLogic::Test(-7.0, EPCGExComparison::EqualOrGreater, -5.0));

	// Zero comparisons
	TestTrue(TEXT("0 >= 0"), NumericCompareLogic::Test(0.0, EPCGExComparison::EqualOrGreater, 0.0));
	TestTrue(TEXT("0 >= -1"), NumericCompareLogic::Test(0.0, EPCGExComparison::EqualOrGreater, -1.0));
	TestFalse(TEXT("0 not >= 1"), NumericCompareLogic::Test(0.0, EPCGExComparison::EqualOrGreater, 1.0));

	return true;
}

// =============================================================================
// EqualOrSmaller Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareEqualOrSmallerTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.EqualOrSmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareEqualOrSmallerTest::RunTest(const FString& Parameters)
{
	// Equal case
	TestTrue(TEXT("5 <= 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::EqualOrSmaller, 5.0));

	// Smaller case
	TestTrue(TEXT("3 <= 5"), NumericCompareLogic::Test(3.0, EPCGExComparison::EqualOrSmaller, 5.0));

	// Greater case (fails)
	TestFalse(TEXT("10 not <= 5"), NumericCompareLogic::Test(10.0, EPCGExComparison::EqualOrSmaller, 5.0));

	// Negative numbers
	TestTrue(TEXT("-5 <= -3"), NumericCompareLogic::Test(-5.0, EPCGExComparison::EqualOrSmaller, -3.0));

	return true;
}

// =============================================================================
// StrictlyGreater Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareStrictlyGreaterTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.StrictlyGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareStrictlyGreaterTest::RunTest(const FString& Parameters)
{
	// Greater case
	TestTrue(TEXT("10 > 5"), NumericCompareLogic::Test(10.0, EPCGExComparison::StrictlyGreater, 5.0));

	// Equal case (fails - must be strictly greater)
	TestFalse(TEXT("5 not > 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlyGreater, 5.0));

	// Less case (fails)
	TestFalse(TEXT("3 not > 5"), NumericCompareLogic::Test(3.0, EPCGExComparison::StrictlyGreater, 5.0));

	// Very small difference
	TestTrue(TEXT("5.0001 > 5.0"), NumericCompareLogic::Test(5.0001, EPCGExComparison::StrictlyGreater, 5.0));

	return true;
}

// =============================================================================
// StrictlySmaller Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareSrictlySmallerTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.StrictlySmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareSrictlySmallerTest::RunTest(const FString& Parameters)
{
	// Smaller case
	TestTrue(TEXT("3 < 5"), NumericCompareLogic::Test(3.0, EPCGExComparison::StrictlySmaller, 5.0));

	// Equal case (fails - must be strictly smaller)
	TestFalse(TEXT("5 not < 5"), NumericCompareLogic::Test(5.0, EPCGExComparison::StrictlySmaller, 5.0));

	// Greater case (fails)
	TestFalse(TEXT("10 not < 5"), NumericCompareLogic::Test(10.0, EPCGExComparison::StrictlySmaller, 5.0));

	return true;
}

// =============================================================================
// NearlyEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareNearlyEqualTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.NearlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareNearlyEqualTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Within tolerance
	TestTrue(TEXT("5.05 ~= 5.0 (tol 0.1)"),
		NumericCompareLogic::Test(5.05, EPCGExComparison::NearlyEqual, 5.0, Tolerance));

	// At tolerance boundary
	TestTrue(TEXT("5.09 ~= 5.0 (tol 0.1)"),
		NumericCompareLogic::Test(5.09, EPCGExComparison::NearlyEqual, 5.0, Tolerance));

	// Outside tolerance
	TestFalse(TEXT("5.2 !~= 5.0 (tol 0.1)"),
		NumericCompareLogic::Test(5.2, EPCGExComparison::NearlyEqual, 5.0, Tolerance));

	// Floating point precision fix
	double Sum = 0.1 + 0.2;
	TestTrue(TEXT("0.1+0.2 ~= 0.3"),
		NumericCompareLogic::Test(Sum, EPCGExComparison::NearlyEqual, 0.3, 0.0001));

	// Negative values
	TestTrue(TEXT("-5.05 ~= -5.0 (tol 0.1)"),
		NumericCompareLogic::Test(-5.05, EPCGExComparison::NearlyEqual, -5.0, Tolerance));

	// Zero comparison
	TestTrue(TEXT("0.05 ~= 0 (tol 0.1)"),
		NumericCompareLogic::Test(0.05, EPCGExComparison::NearlyEqual, 0.0, Tolerance));

	return true;
}

// =============================================================================
// NearlyNotEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareNearlyNotEqualTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.NearlyNotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareNearlyNotEqualTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Outside tolerance (passes)
	TestTrue(TEXT("5.2 !~= 5.0 (tol 0.1)"),
		NumericCompareLogic::Test(5.2, EPCGExComparison::NearlyNotEqual, 5.0, Tolerance));

	// Within tolerance (fails)
	TestFalse(TEXT("5.05 not !~= 5.0 (tol 0.1)"),
		NumericCompareLogic::Test(5.05, EPCGExComparison::NearlyNotEqual, 5.0, Tolerance));

	// Exactly equal
	TestFalse(TEXT("5.0 not !~= 5.0"),
		NumericCompareLogic::Test(5.0, EPCGExComparison::NearlyNotEqual, 5.0, Tolerance));

	// Large difference
	TestTrue(TEXT("100 !~= 5.0"),
		NumericCompareLogic::Test(100.0, EPCGExComparison::NearlyNotEqual, 5.0, Tolerance));

	return true;
}

// =============================================================================
// Tolerance Behavior Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareToleranceTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.Tolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareToleranceTest::RunTest(const FString& Parameters)
{
	// Very tight tolerance
	const double TightTol = 0.0001;
	TestTrue(TEXT("Tight: 5.00005 ~= 5.0"),
		NumericCompareLogic::Test(5.00005, EPCGExComparison::NearlyEqual, 5.0, TightTol));
	TestFalse(TEXT("Tight: 5.001 !~= 5.0"),
		NumericCompareLogic::Test(5.001, EPCGExComparison::NearlyEqual, 5.0, TightTol));

	// Loose tolerance
	const double LooseTol = 1.0;
	TestTrue(TEXT("Loose: 5.5 ~= 5.0"),
		NumericCompareLogic::Test(5.5, EPCGExComparison::NearlyEqual, 5.0, LooseTol));
	TestTrue(TEXT("Loose: 5.99 ~= 5.0"),
		NumericCompareLogic::Test(5.99, EPCGExComparison::NearlyEqual, 5.0, LooseTol));
	TestFalse(TEXT("Loose: 6.5 !~= 5.0"),
		NumericCompareLogic::Test(6.5, EPCGExComparison::NearlyEqual, 5.0, LooseTol));

	// Zero tolerance (exact match only)
	const double ZeroTol = 0.0;
	TestTrue(TEXT("Zero tol: 5.0 ~= 5.0"),
		NumericCompareLogic::Test(5.0, EPCGExComparison::NearlyEqual, 5.0, ZeroTol));
	// Very tiny difference with zero tolerance may still pass due to FMath::IsNearlyEqual implementation
	// but 0.1 difference should fail
	TestFalse(TEXT("Zero tol: 5.1 !~= 5.0"),
		NumericCompareLogic::Test(5.1, EPCGExComparison::NearlyEqual, 5.0, ZeroTol));

	return true;
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareEdgeCasesTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareEdgeCasesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Very large numbers
	const double Large = 1e15;
	TestTrue(TEXT("Large == Large"),
		NumericCompareLogic::Test(Large, EPCGExComparison::StrictlyEqual, Large));
	TestTrue(TEXT("Large+1 > Large"),
		NumericCompareLogic::Test(Large + 1, EPCGExComparison::StrictlyGreater, Large));

	// Very small numbers
	const double Small = 1e-15;
	TestTrue(TEXT("Small == Small"),
		NumericCompareLogic::Test(Small, EPCGExComparison::StrictlyEqual, Small));

	// Infinity
	const double Inf = std::numeric_limits<double>::infinity();
	TestTrue(TEXT("Inf == Inf"),
		NumericCompareLogic::Test(Inf, EPCGExComparison::StrictlyEqual, Inf));
	TestTrue(TEXT("Inf > 1e308"),
		NumericCompareLogic::Test(Inf, EPCGExComparison::StrictlyGreater, 1e308));
	TestTrue(TEXT("-Inf < Inf"),
		NumericCompareLogic::Test(-Inf, EPCGExComparison::StrictlySmaller, Inf));

	// Negative zero
	TestTrue(TEXT("-0.0 == 0.0"),
		NumericCompareLogic::Test(-0.0, EPCGExComparison::StrictlyEqual, 0.0));

	// Subnormal numbers
	const double Subnormal = std::numeric_limits<double>::denorm_min();
	TestTrue(TEXT("Subnormal > 0"),
		NumericCompareLogic::Test(Subnormal, EPCGExComparison::StrictlyGreater, 0.0));

	return true;
}

// =============================================================================
// Integer-like Values Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareIntegerLikeTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.IntegerLike",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareIntegerLikeTest::RunTest(const FString& Parameters)
{
	// Test behavior when using doubles to represent integers
	// (common pattern when attribute values are int32 converted to double)

	// Sequential integers
	for (int32 i = 0; i < 10; ++i)
	{
		double Val = static_cast<double>(i);
		TestTrue(FString::Printf(TEXT("%d == %d"), i, i),
			NumericCompareLogic::Test(Val, EPCGExComparison::StrictlyEqual, Val));
	}

	// Even/odd filtering (common pattern)
	// Points with index % 2 == 0
	for (int32 i = 0; i < 10; ++i)
	{
		double Index = static_cast<double>(i);
		double ModResult = FMath::Fmod(Index, 2.0);
		bool IsEven = (ModResult == 0.0);

		if (i % 2 == 0)
		{
			TestTrue(FString::Printf(TEXT("%d is even"), i), IsEven);
		}
		else
		{
			TestFalse(FString::Printf(TEXT("%d is not even"), i), IsEven);
		}
	}

	// Negative integers
	TestTrue(TEXT("-100 < -50"),
		NumericCompareLogic::Test(-100.0, EPCGExComparison::StrictlySmaller, -50.0));
	TestTrue(TEXT("-50 > -100"),
		NumericCompareLogic::Test(-50.0, EPCGExComparison::StrictlyGreater, -100.0));

	return true;
}

// =============================================================================
// Practical Scenarios Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExNumericCompareScenariosTest,
	"PCGEx.Unit.Filters.NumericCompareLogic.Scenarios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExNumericCompareScenariosTest::RunTest(const FString& Parameters)
{
	// Scenario 1: Height filter (points above ground level)
	{
		const double GroundLevel = 100.0;
		const double PointAbove = 150.0;
		const double PointBelow = 50.0;
		const double PointAt = 100.0;

		TestTrue(TEXT("Point 150 > ground 100"),
			NumericCompareLogic::Test(PointAbove, EPCGExComparison::StrictlyGreater, GroundLevel));
		TestFalse(TEXT("Point 50 not > ground 100"),
			NumericCompareLogic::Test(PointBelow, EPCGExComparison::StrictlyGreater, GroundLevel));
		TestTrue(TEXT("Point 100 >= ground 100"),
			NumericCompareLogic::Test(PointAt, EPCGExComparison::EqualOrGreater, GroundLevel));
	}

	// Scenario 2: Distance threshold (points within range)
	{
		const double MaxDistance = 500.0;
		const double Tolerance = 1.0;

		const double Close = 100.0;
		const double AtBoundary = 500.0;
		const double Far = 600.0;

		TestTrue(TEXT("Close point within range"),
			NumericCompareLogic::Test(Close, EPCGExComparison::EqualOrSmaller, MaxDistance));
		TestTrue(TEXT("Boundary point within range"),
			NumericCompareLogic::Test(AtBoundary, EPCGExComparison::EqualOrSmaller, MaxDistance));
		TestFalse(TEXT("Far point outside range"),
			NumericCompareLogic::Test(Far, EPCGExComparison::EqualOrSmaller, MaxDistance));
	}

	// Scenario 3: Weight threshold (points with sufficient weight)
	{
		const double MinWeight = 0.5;
		const double Tolerance = 0.05;

		const double HighWeight = 0.8;
		const double LowWeight = 0.3;
		const double BoundaryWeight = 0.52;

		TestTrue(TEXT("High weight passes threshold"),
			NumericCompareLogic::Test(HighWeight, EPCGExComparison::EqualOrGreater, MinWeight));
		TestFalse(TEXT("Low weight fails threshold"),
			NumericCompareLogic::Test(LowWeight, EPCGExComparison::EqualOrGreater, MinWeight));
		TestTrue(TEXT("Boundary weight with tolerance"),
			NumericCompareLogic::Test(BoundaryWeight, EPCGExComparison::NearlyEqual, MinWeight, Tolerance));
	}

	// Scenario 4: Classification by value bands
	{
		const double Value = 75.0;

		// Check which band the value falls into
		bool Band0To25 = NumericCompareLogic::Test(Value, EPCGExComparison::StrictlySmaller, 25.0);
		bool Band25To50 = NumericCompareLogic::Test(Value, EPCGExComparison::EqualOrGreater, 25.0) &&
		                  NumericCompareLogic::Test(Value, EPCGExComparison::StrictlySmaller, 50.0);
		bool Band50To75 = NumericCompareLogic::Test(Value, EPCGExComparison::EqualOrGreater, 50.0) &&
		                  NumericCompareLogic::Test(Value, EPCGExComparison::StrictlySmaller, 75.0);
		bool Band75To100 = NumericCompareLogic::Test(Value, EPCGExComparison::EqualOrGreater, 75.0) &&
		                   NumericCompareLogic::Test(Value, EPCGExComparison::EqualOrSmaller, 100.0);

		TestFalse(TEXT("75 not in [0,25)"), Band0To25);
		TestFalse(TEXT("75 not in [25,50)"), Band25To50);
		TestFalse(TEXT("75 not in [50,75)"), Band50To75);
		TestTrue(TEXT("75 in [75,100]"), Band75To100);
	}

	return true;
}
