// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Filter Logic Unit Tests
 *
 * Tests filter logic simulation without PCG context.
 * These tests verify the mathematical/logical behavior of filters
 * by simulating their Test() logic with known inputs.
 *
 * Covered filters:
 * - Boolean Compare: A == B or A != B for booleans
 * - Within Range: Value in [min, max] with inclusive/exclusive options
 * - Modulo Compare: (A % B) comparison C
 *
 * Test naming convention: PCGEx.Unit.Filters.<FilterName>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Utils/PCGExCompare.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Boolean Compare Filter Logic Tests
// =============================================================================

/**
 * Simulates the boolean compare filter logic.
 * Matches the actual filter Test() implementation.
 */
namespace BooleanCompareLogic
{
	bool Test(bool OperandA, EPCGExEquality Comparison, bool OperandB)
	{
		switch (Comparison)
		{
		case EPCGExEquality::Equal:
			return OperandA == OperandB;
		case EPCGExEquality::NotEqual:
			return OperandA != OperandB;
		default:
			return false;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBooleanCompareEqualTrueTest,
	"PCGEx.Unit.Filters.BooleanCompare.Equal.True",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBooleanCompareEqualTrueTest::RunTest(const FString& Parameters)
{
	// Test all combinations for Equal comparison
	TestTrue(TEXT("true == true"),
		BooleanCompareLogic::Test(true, EPCGExEquality::Equal, true));
	TestTrue(TEXT("false == false"),
		BooleanCompareLogic::Test(false, EPCGExEquality::Equal, false));
	TestFalse(TEXT("true != false (for == comparison)"),
		BooleanCompareLogic::Test(true, EPCGExEquality::Equal, false));
	TestFalse(TEXT("false != true (for == comparison)"),
		BooleanCompareLogic::Test(false, EPCGExEquality::Equal, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBooleanCompareNotEqualTest,
	"PCGEx.Unit.Filters.BooleanCompare.NotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBooleanCompareNotEqualTest::RunTest(const FString& Parameters)
{
	// Test all combinations for NotEqual comparison
	TestFalse(TEXT("true !not!= true"),
		BooleanCompareLogic::Test(true, EPCGExEquality::NotEqual, true));
	TestFalse(TEXT("false !not!= false"),
		BooleanCompareLogic::Test(false, EPCGExEquality::NotEqual, false));
	TestTrue(TEXT("true != false"),
		BooleanCompareLogic::Test(true, EPCGExEquality::NotEqual, false));
	TestTrue(TEXT("false != true"),
		BooleanCompareLogic::Test(false, EPCGExEquality::NotEqual, true));

	return true;
}

// =============================================================================
// Within Range Filter Logic Tests
// =============================================================================

/**
 * Simulates the within range filter logic.
 * Matches the actual filter Test() implementation.
 */
namespace WithinRangeLogic
{
	bool Test(double Value, double RangeMin, double RangeMax, bool bInclusive, bool bInvert)
	{
		bool bWithin;
		if (bInclusive)
		{
			bWithin = (Value >= RangeMin && Value <= RangeMax);
		}
		else
		{
			bWithin = (Value > RangeMin && Value < RangeMax);
		}

		return bInvert ? !bWithin : bWithin;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWithinRangeInclusiveTest,
	"PCGEx.Unit.Filters.WithinRange.Inclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWithinRangeInclusiveTest::RunTest(const FString& Parameters)
{
	const double Min = 0.0;
	const double Max = 10.0;
	const bool bInclusive = true;
	const bool bInvert = false;

	// Inside range
	TestTrue(TEXT("5 is within [0, 10]"),
		WithinRangeLogic::Test(5.0, Min, Max, bInclusive, bInvert));

	// At boundaries (inclusive)
	TestTrue(TEXT("0 is within [0, 10] (inclusive)"),
		WithinRangeLogic::Test(0.0, Min, Max, bInclusive, bInvert));
	TestTrue(TEXT("10 is within [0, 10] (inclusive)"),
		WithinRangeLogic::Test(10.0, Min, Max, bInclusive, bInvert));

	// Outside range
	TestFalse(TEXT("-1 is not within [0, 10]"),
		WithinRangeLogic::Test(-1.0, Min, Max, bInclusive, bInvert));
	TestFalse(TEXT("11 is not within [0, 10]"),
		WithinRangeLogic::Test(11.0, Min, Max, bInclusive, bInvert));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWithinRangeExclusiveTest,
	"PCGEx.Unit.Filters.WithinRange.Exclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWithinRangeExclusiveTest::RunTest(const FString& Parameters)
{
	const double Min = 0.0;
	const double Max = 10.0;
	const bool bInclusive = false;
	const bool bInvert = false;

	// Inside range
	TestTrue(TEXT("5 is within (0, 10)"),
		WithinRangeLogic::Test(5.0, Min, Max, bInclusive, bInvert));

	// At boundaries (exclusive - should fail)
	TestFalse(TEXT("0 is not within (0, 10) exclusive"),
		WithinRangeLogic::Test(0.0, Min, Max, bInclusive, bInvert));
	TestFalse(TEXT("10 is not within (0, 10) exclusive"),
		WithinRangeLogic::Test(10.0, Min, Max, bInclusive, bInvert));

	// Just inside boundaries
	TestTrue(TEXT("0.001 is within (0, 10)"),
		WithinRangeLogic::Test(0.001, Min, Max, bInclusive, bInvert));
	TestTrue(TEXT("9.999 is within (0, 10)"),
		WithinRangeLogic::Test(9.999, Min, Max, bInclusive, bInvert));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWithinRangeInvertTest,
	"PCGEx.Unit.Filters.WithinRange.Invert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWithinRangeInvertTest::RunTest(const FString& Parameters)
{
	const double Min = 0.0;
	const double Max = 10.0;
	const bool bInclusive = true;
	const bool bInvert = true;

	// With invert: inside becomes outside
	TestFalse(TEXT("5 is NOT within [0, 10] when inverted"),
		WithinRangeLogic::Test(5.0, Min, Max, bInclusive, bInvert));

	// With invert: outside becomes inside
	TestTrue(TEXT("-1 passes when inverted (outside range)"),
		WithinRangeLogic::Test(-1.0, Min, Max, bInclusive, bInvert));
	TestTrue(TEXT("11 passes when inverted (outside range)"),
		WithinRangeLogic::Test(11.0, Min, Max, bInclusive, bInvert));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWithinRangeNegativeTest,
	"PCGEx.Unit.Filters.WithinRange.Negative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWithinRangeNegativeTest::RunTest(const FString& Parameters)
{
	const double Min = -100.0;
	const double Max = -50.0;
	const bool bInclusive = true;
	const bool bInvert = false;

	TestTrue(TEXT("-75 is within [-100, -50]"),
		WithinRangeLogic::Test(-75.0, Min, Max, bInclusive, bInvert));
	TestTrue(TEXT("-100 is within [-100, -50] (inclusive)"),
		WithinRangeLogic::Test(-100.0, Min, Max, bInclusive, bInvert));
	TestFalse(TEXT("0 is not within [-100, -50]"),
		WithinRangeLogic::Test(0.0, Min, Max, bInclusive, bInvert));
	TestFalse(TEXT("-25 is not within [-100, -50]"),
		WithinRangeLogic::Test(-25.0, Min, Max, bInclusive, bInvert));

	return true;
}

// =============================================================================
// Modulo Compare Filter Logic Tests
// =============================================================================

/**
 * Simulates the modulo compare filter logic.
 * Matches the actual filter Test() implementation: (A % B) comparison C
 */
namespace ModuloCompareLogic
{
	bool Test(double A, double B, EPCGExComparison Comparison, double C, double Tolerance, bool ZeroResult)
	{
		// Handle division by zero
		if (FMath::IsNearlyZero(B))
		{
			return ZeroResult;
		}

		double ModResult = FMath::Fmod(A, B);
		return PCGExCompare::Compare(Comparison, ModResult, C, Tolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareBasicTest,
	"PCGEx.Unit.Filters.ModuloCompare.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareBasicTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// 10 % 3 = 1
	TestTrue(TEXT("10 % 3 == 1"),
		ModuloCompareLogic::Test(10.0, 3.0, EPCGExComparison::StrictlyEqual, 1.0, Tolerance, true));

	// 10 % 5 = 0
	TestTrue(TEXT("10 % 5 == 0"),
		ModuloCompareLogic::Test(10.0, 5.0, EPCGExComparison::StrictlyEqual, 0.0, Tolerance, true));

	// 7 % 4 = 3
	TestTrue(TEXT("7 % 4 == 3"),
		ModuloCompareLogic::Test(7.0, 4.0, EPCGExComparison::StrictlyEqual, 3.0, Tolerance, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareComparisonTypesTest,
	"PCGEx.Unit.Filters.ModuloCompare.ComparisonTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareComparisonTypesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// 10 % 4 = 2
	TestTrue(TEXT("10 % 4 >= 1"),
		ModuloCompareLogic::Test(10.0, 4.0, EPCGExComparison::EqualOrGreater, 1.0, Tolerance, true));
	TestTrue(TEXT("10 % 4 <= 3"),
		ModuloCompareLogic::Test(10.0, 4.0, EPCGExComparison::EqualOrSmaller, 3.0, Tolerance, true));
	TestTrue(TEXT("10 % 4 > 1"),
		ModuloCompareLogic::Test(10.0, 4.0, EPCGExComparison::StrictlyGreater, 1.0, Tolerance, true));
	TestTrue(TEXT("10 % 4 < 3"),
		ModuloCompareLogic::Test(10.0, 4.0, EPCGExComparison::StrictlySmaller, 3.0, Tolerance, true));
	TestTrue(TEXT("10 % 4 != 0"),
		ModuloCompareLogic::Test(10.0, 4.0, EPCGExComparison::StrictlyNotEqual, 0.0, Tolerance, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareNearlyEqualTest,
	"PCGEx.Unit.Filters.ModuloCompare.NearlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareNearlyEqualTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// 10.05 % 3 ~= 1.05, which is ~= 1.0 within tolerance
	TestTrue(TEXT("10.05 % 3 ~= 1.0 (tolerance 0.1)"),
		ModuloCompareLogic::Test(10.05, 3.0, EPCGExComparison::NearlyEqual, 1.0, Tolerance, true));

	// With tight tolerance, should fail
	TestFalse(TEXT("10.05 % 3 !~= 1.0 (tolerance 0.01)"),
		ModuloCompareLogic::Test(10.05, 3.0, EPCGExComparison::NearlyEqual, 1.0, 0.01, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareZeroDivisorTest,
	"PCGEx.Unit.Filters.ModuloCompare.ZeroDivisor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareZeroDivisorTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// When B is zero, return ZeroResult
	TestTrue(TEXT("A % 0 returns ZeroResult=true"),
		ModuloCompareLogic::Test(10.0, 0.0, EPCGExComparison::StrictlyEqual, 0.0, Tolerance, true));
	TestFalse(TEXT("A % 0 returns ZeroResult=false"),
		ModuloCompareLogic::Test(10.0, 0.0, EPCGExComparison::StrictlyEqual, 0.0, Tolerance, false));

	// Near-zero should also trigger
	TestTrue(TEXT("A % ~0 returns ZeroResult=true"),
		ModuloCompareLogic::Test(10.0, 1e-15, EPCGExComparison::StrictlyEqual, 0.0, Tolerance, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareNegativeTest,
	"PCGEx.Unit.Filters.ModuloCompare.Negative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareNegativeTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Negative modulo behavior (FMath::Fmod preserves sign of dividend)
	// -10 % 3 = -1 in C++ Fmod
	double Result = FMath::Fmod(-10.0, 3.0);
	TestTrue(TEXT("-10 % 3 result is negative"),
		Result < 0);

	TestTrue(TEXT("-10 % 3 == -1"),
		ModuloCompareLogic::Test(-10.0, 3.0, EPCGExComparison::StrictlyEqual, -1.0, Tolerance, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExModuloCompareFloatTest,
	"PCGEx.Unit.Filters.ModuloCompare.Float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExModuloCompareFloatTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Floating point modulo
	// 5.5 % 2.0 = 1.5
	TestTrue(TEXT("5.5 % 2.0 == 1.5"),
		ModuloCompareLogic::Test(5.5, 2.0, EPCGExComparison::NearlyEqual, 1.5, Tolerance, true));

	// 7.3 % 2.5 = 2.3
	TestTrue(TEXT("7.3 % 2.5 ~= 2.3"),
		ModuloCompareLogic::Test(7.3, 2.5, EPCGExComparison::NearlyEqual, 2.3, Tolerance, true));

	return true;
}

// =============================================================================
// Edge Cases and Combined Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExFilterLogicEdgeCasesTest,
	"PCGEx.Unit.Filters.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExFilterLogicEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Zero-width range
	TestTrue(TEXT("5 within [5, 5] inclusive"),
		WithinRangeLogic::Test(5.0, 5.0, 5.0, true, false));
	TestFalse(TEXT("5 not within (5, 5) exclusive"),
		WithinRangeLogic::Test(5.0, 5.0, 5.0, false, false));

	// Very large numbers
	const double Large = 1e15;
	TestTrue(TEXT("Large value modulo"),
		ModuloCompareLogic::Test(Large, 7.0, EPCGExComparison::EqualOrGreater, 0.0, 0.01, true));

	// Very small numbers
	const double Small = 1e-10;
	TestTrue(TEXT("Small value within range"),
		WithinRangeLogic::Test(Small, 0.0, 1e-9, true, false));

	return true;
}
