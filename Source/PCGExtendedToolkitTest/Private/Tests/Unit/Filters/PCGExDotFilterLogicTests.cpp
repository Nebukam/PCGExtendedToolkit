// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Dot Filter Logic Unit Tests
 *
 * Tests dot product comparison logic without PCG context.
 * Simulates the FPCGExDotComparisonDetails::Test() behavior.
 *
 * Covered scenarios:
 * - Dot product comparison with various thresholds
 * - Scalar domain (raw dot: -1 to 1)
 * - Degrees domain (angular: 0 to 180)
 * - Unsigned comparison (absolute value)
 * - All comparison operators
 *
 * Test naming convention: PCGEx.Unit.Filters.DotLogic.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Utils/PCGExCompare.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Dot Comparison Logic Simulation
// =============================================================================

/**
 * Simulates the dot comparison logic from FPCGExDotComparisonDetails.
 * This matches the actual Test() implementation.
 */
namespace DotCompareLogic
{
	/**
	 * Convert degrees to dot product value.
	 * 0 degrees = dot 1 (same direction)
	 * 90 degrees = dot 0 (perpendicular)
	 * 180 degrees = dot -1 (opposite)
	 */
	FORCEINLINE double DegreesToDot(const double Degrees)
	{
		return FMath::Cos(FMath::DegreesToRadians(Degrees));
	}

	/**
	 * Convert dot product to degrees.
	 */
	FORCEINLINE double DotToDegrees(const double Dot)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0)));
	}

	/**
	 * Test a dot product against a threshold.
	 * Matches FPCGExDotComparisonDetails::Test() behavior.
	 *
	 * @param DotProduct - The raw dot product value (-1 to 1)
	 * @param Threshold - The threshold to compare against (in scalar or degrees domain)
	 * @param Domain - Whether threshold is Scalar or Degrees
	 * @param Comparison - The comparison operator
	 * @param Tolerance - Tolerance for nearly equal comparisons
	 * @param bUnsigned - If true, use absolute value of dot product
	 */
	bool Test(
		double DotProduct,
		double Threshold,
		EPCGExAngularDomain Domain,
		EPCGExComparison Comparison,
		double Tolerance,
		bool bUnsigned = false)
	{
		// Convert threshold to scalar domain if needed
		// NOTE: For degrees, the actual implementation uses (180 - Degrees) to invert semantics
		// so that "angle <= threshold" works intuitively (smaller angle = more aligned = higher dot)
		double ScalarThreshold = Threshold;
		if (Domain == EPCGExAngularDomain::Degrees)
		{
			ScalarThreshold = DegreesToDot(180.0 - Threshold);
		}

		// The actual Test() remaps both values from [-1,1] to [0,1] for comparison
		// This preserves semantics while normalizing the range
		if (bUnsigned)
		{
			return PCGExCompare::Compare(Comparison, FMath::Abs(DotProduct), FMath::Abs(ScalarThreshold), Tolerance);
		}
		else
		{
			return PCGExCompare::Compare(Comparison, (1.0 + DotProduct) * 0.5, (1.0 + ScalarThreshold) * 0.5, Tolerance);
		}
	}

	/**
	 * Compute dot product between two vectors.
	 */
	FORCEINLINE double ComputeDot(const FVector& A, const FVector& B)
	{
		return FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal());
	}
}

// =============================================================================
// Scalar Domain Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicScalarEqualOrGreaterTest,
	"PCGEx.Unit.Filters.DotLogic.Scalar.EqualOrGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicScalarEqualOrGreaterTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Same direction (dot = 1.0)
	TestTrue(TEXT("Same direction (1.0) >= 0.5"),
		DotCompareLogic::Test(1.0, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// At threshold
	TestTrue(TEXT("At threshold (0.5) >= 0.5"),
		DotCompareLogic::Test(0.5, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// Below threshold
	TestFalse(TEXT("Below threshold (0.3) >= 0.5"),
		DotCompareLogic::Test(0.3, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// Perpendicular (dot = 0)
	TestFalse(TEXT("Perpendicular (0) >= 0.5"),
		DotCompareLogic::Test(0.0, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// Opposite direction (dot = -1)
	TestFalse(TEXT("Opposite (-1) >= 0"),
		DotCompareLogic::Test(-1.0, 0.0, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicScalarStrictlyGreaterTest,
	"PCGEx.Unit.Filters.DotLogic.Scalar.StrictlyGreater",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicScalarStrictlyGreaterTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Above threshold
	TestTrue(TEXT("Above threshold (0.8) > 0.5"),
		DotCompareLogic::Test(0.8, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyGreater, Tolerance));

	// At threshold (should fail for strictly greater)
	TestFalse(TEXT("At threshold (0.5) not > 0.5"),
		DotCompareLogic::Test(0.5, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyGreater, Tolerance));

	// Below threshold
	TestFalse(TEXT("Below threshold (0.3) not > 0.5"),
		DotCompareLogic::Test(0.3, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyGreater, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicScalarNearlyEqualTest,
	"PCGEx.Unit.Filters.DotLogic.Scalar.NearlyEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicScalarNearlyEqualTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.05;

	// Note: Values are remapped via (1+x)*0.5 before comparison
	// So 0.5 → 0.75, 0.52 → 0.76, 0.6 → 0.8, etc.
	// Difference of 0.02 in original → 0.01 in remapped

	// Within tolerance: 0.52 vs 0.5 → remapped 0.76 vs 0.75 = diff 0.01 < 0.05
	TestTrue(TEXT("Within tolerance (0.52 ~= 0.5)"),
		DotCompareLogic::Test(0.52, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::NearlyEqual, Tolerance));

	// Near boundary: 0.58 vs 0.5 → remapped 0.79 vs 0.75 = diff 0.04 < 0.05
	TestTrue(TEXT("Near tolerance boundary (0.58 ~= 0.5)"),
		DotCompareLogic::Test(0.58, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::NearlyEqual, Tolerance));

	// Outside tolerance: 0.8 vs 0.5 → remapped 0.9 vs 0.75 = diff 0.15 > 0.05
	TestFalse(TEXT("Outside tolerance (0.8 !~= 0.5)"),
		DotCompareLogic::Test(0.8, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::NearlyEqual, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicScalarComparisonOpsTest,
	"PCGEx.Unit.Filters.DotLogic.Scalar.AllOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicScalarComparisonOpsTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;
	const double DotValue = 0.7;
	const double Threshold = 0.5;

	// ==
	TestFalse(TEXT("0.7 != 0.5"),
		DotCompareLogic::Test(DotValue, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyEqual, Tolerance));
	TestTrue(TEXT("0.5 == 0.5"),
		DotCompareLogic::Test(0.5, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyEqual, Tolerance));

	// !=
	TestTrue(TEXT("0.7 != 0.5 (NotEqual)"),
		DotCompareLogic::Test(DotValue, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyNotEqual, Tolerance));

	// >=
	TestTrue(TEXT("0.7 >= 0.5"),
		DotCompareLogic::Test(DotValue, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// <=
	TestFalse(TEXT("0.7 not <= 0.5"),
		DotCompareLogic::Test(DotValue, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrSmaller, Tolerance));
	TestTrue(TEXT("0.3 <= 0.5"),
		DotCompareLogic::Test(0.3, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrSmaller, Tolerance));

	// >
	TestTrue(TEXT("0.7 > 0.5"),
		DotCompareLogic::Test(DotValue, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyGreater, Tolerance));

	// <
	TestTrue(TEXT("0.3 < 0.5"),
		DotCompareLogic::Test(0.3, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlySmaller, Tolerance));

	return true;
}

// =============================================================================
// Degrees Domain Tests
// =============================================================================

/**
 * IMPORTANT: Degrees domain semantics
 *
 * The transformation DegreesToDot(180 - threshold) means:
 * - threshold T° creates a boundary at angle (180-T)°
 * - threshold 45° → boundary at 135° (NOT 45°!)
 * - threshold 90° → boundary at 90° (symmetric case)
 * - threshold 135° → boundary at 45°
 *
 * With EqualOrGreater:
 * - threshold 90° accepts angles 0° to 90° (within 90° of alignment)
 * - threshold 45° accepts angles 0° to 135° (within 135° of alignment)
 *
 * For intuitive "within X degrees" behavior, use threshold = (180 - X):
 * - "within 45°" → use threshold 135° with EqualOrGreater
 * - "within 90°" → use threshold 90° with EqualOrGreater
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicDegreesBasicTest,
	"PCGEx.Unit.Filters.DotLogic.Degrees.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicDegreesBasicTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 1.0;

	// Test with threshold 90° (symmetric case where semantics are clearest)
	// EqualOrGreater with 90° = "angle within 90° of alignment"

	// 0° (dot=1.0) vs threshold 90° - should pass (0° is within 90°)
	TestTrue(TEXT("0 deg passes 90 deg threshold (EqualOrGreater)"),
		DotCompareLogic::Test(1.0, 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// 45° (dot~=0.707) vs threshold 90° - should pass (45° is within 90°)
	TestTrue(TEXT("45 deg passes 90 deg threshold"),
		DotCompareLogic::Test(DotCompareLogic::DegreesToDot(45.0), 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// 90° (dot=0) vs threshold 90° - boundary case, should pass
	TestTrue(TEXT("90 deg passes 90 deg threshold (boundary)"),
		DotCompareLogic::Test(0.0, 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// 135° (dot~=-0.707) vs threshold 90° - should fail (135° is NOT within 90°)
	TestFalse(TEXT("135 deg fails 90 deg threshold"),
		DotCompareLogic::Test(DotCompareLogic::DegreesToDot(135.0), 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// 180° (dot=-1) vs threshold 90° - should fail (180° is NOT within 90°)
	TestFalse(TEXT("180 deg fails 90 deg threshold"),
		DotCompareLogic::Test(-1.0, 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrGreater, Tolerance));

	// Test EqualOrSmaller with 90° = "angle outside 90° of alignment"
	TestFalse(TEXT("0 deg fails 90 deg threshold (EqualOrSmaller)"),
		DotCompareLogic::Test(1.0, 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrSmaller, Tolerance));

	TestTrue(TEXT("135 deg passes 90 deg threshold (EqualOrSmaller)"),
		DotCompareLogic::Test(DotCompareLogic::DegreesToDot(135.0), 90.0, EPCGExAngularDomain::Degrees,
			EPCGExComparison::EqualOrSmaller, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicDegreesConversionTest,
	"PCGEx.Unit.Filters.DotLogic.Degrees.Conversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicDegreesConversionTest::RunTest(const FString& Parameters)
{
	// Test degree-to-dot conversion accuracy
	const double Tol = 0.001;

	// 0 degrees = dot 1
	TestTrue(TEXT("0 deg -> dot 1"),
		FMath::IsNearlyEqual(DotCompareLogic::DegreesToDot(0.0), 1.0, Tol));

	// 90 degrees = dot 0
	TestTrue(TEXT("90 deg -> dot 0"),
		FMath::IsNearlyEqual(DotCompareLogic::DegreesToDot(90.0), 0.0, Tol));

	// 180 degrees = dot -1
	TestTrue(TEXT("180 deg -> dot -1"),
		FMath::IsNearlyEqual(DotCompareLogic::DegreesToDot(180.0), -1.0, Tol));

	// 60 degrees = dot 0.5
	TestTrue(TEXT("60 deg -> dot 0.5"),
		FMath::IsNearlyEqual(DotCompareLogic::DegreesToDot(60.0), 0.5, Tol));

	// 120 degrees = dot -0.5
	TestTrue(TEXT("120 deg -> dot -0.5"),
		FMath::IsNearlyEqual(DotCompareLogic::DegreesToDot(120.0), -0.5, Tol));

	// Reverse conversion
	TestTrue(TEXT("dot 1 -> 0 deg"),
		FMath::IsNearlyEqual(DotCompareLogic::DotToDegrees(1.0), 0.0, Tol));
	TestTrue(TEXT("dot 0 -> 90 deg"),
		FMath::IsNearlyEqual(DotCompareLogic::DotToDegrees(0.0), 90.0, Tol));
	TestTrue(TEXT("dot -1 -> 180 deg"),
		FMath::IsNearlyEqual(DotCompareLogic::DotToDegrees(-1.0), 180.0, Tol));

	return true;
}

// =============================================================================
// Unsigned Comparison Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicUnsignedTest,
	"PCGEx.Unit.Filters.DotLogic.Unsigned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicUnsignedTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;
	const double Threshold = 0.5;

	// Unsigned mode: treats opposite directions as equivalent
	// dot = -0.8 -> abs = 0.8

	// Negative dot with unsigned mode
	TestTrue(TEXT("Unsigned: -0.8 >= 0.5 (abs = 0.8)"),
		DotCompareLogic::Test(-0.8, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance, true));

	// Same value signed would fail
	TestFalse(TEXT("Signed: -0.8 < 0.5"),
		DotCompareLogic::Test(-0.8, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance, false));

	// Perpendicular still fails in unsigned mode
	TestFalse(TEXT("Unsigned: 0.0 < 0.5"),
		DotCompareLogic::Test(0.0, Threshold, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance, true));

	// Perfect opposite direction with unsigned
	TestTrue(TEXT("Unsigned: -1.0 == 1.0 (abs)"),
		DotCompareLogic::Test(-1.0, 1.0, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyEqual, Tolerance, true));

	return true;
}

// =============================================================================
// Vector Dot Product Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicVectorTest,
	"PCGEx.Unit.Filters.DotLogic.VectorDot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicVectorTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Same direction
	{
		FVector A(1, 0, 0);
		FVector B(1, 0, 0);
		double Dot = DotCompareLogic::ComputeDot(A, B);
		TestTrue(TEXT("Same direction dot ~= 1"),
			FMath::IsNearlyEqual(Dot, 1.0, Tolerance));
	}

	// Opposite direction
	{
		FVector A(1, 0, 0);
		FVector B(-1, 0, 0);
		double Dot = DotCompareLogic::ComputeDot(A, B);
		TestTrue(TEXT("Opposite direction dot ~= -1"),
			FMath::IsNearlyEqual(Dot, -1.0, Tolerance));
	}

	// Perpendicular
	{
		FVector A(1, 0, 0);
		FVector B(0, 1, 0);
		double Dot = DotCompareLogic::ComputeDot(A, B);
		TestTrue(TEXT("Perpendicular dot ~= 0"),
			FMath::IsNearlyEqual(Dot, 0.0, Tolerance));
	}

	// 45 degrees
	{
		FVector A(1, 0, 0);
		FVector B(1, 1, 0);  // Normalized will be 45 degrees from A
		double Dot = DotCompareLogic::ComputeDot(A, B);
		double Expected = FMath::Cos(FMath::DegreesToRadians(45.0));
		TestTrue(TEXT("45 degree angle"),
			FMath::IsNearlyEqual(Dot, Expected, 0.01));
	}

	// Non-unit vectors (should normalize)
	{
		FVector A(100, 0, 0);
		FVector B(50, 0, 0);
		double Dot = DotCompareLogic::ComputeDot(A, B);
		TestTrue(TEXT("Non-unit same direction dot ~= 1"),
			FMath::IsNearlyEqual(Dot, 1.0, Tolerance));
	}

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicEdgeCasesTest,
	"PCGEx.Unit.Filters.DotLogic.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicEdgeCasesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Threshold at boundaries
	TestTrue(TEXT("dot=1 >= threshold=1"),
		DotCompareLogic::Test(1.0, 1.0, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	TestTrue(TEXT("dot=-1 <= threshold=-1"),
		DotCompareLogic::Test(-1.0, -1.0, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrSmaller, Tolerance));

	// Zero threshold
	TestTrue(TEXT("dot=0.5 > threshold=0"),
		DotCompareLogic::Test(0.5, 0.0, EPCGExAngularDomain::Scalar,
			EPCGExComparison::StrictlyGreater, Tolerance));

	// Very small differences
	TestTrue(TEXT("Very small difference with tolerance"),
		DotCompareLogic::Test(0.5001, 0.5, EPCGExAngularDomain::Scalar,
			EPCGExComparison::NearlyEqual, 0.001));

	// Negative threshold in scalar domain
	TestTrue(TEXT("dot=-0.5 >= threshold=-0.7"),
		DotCompareLogic::Test(-0.5, -0.7, EPCGExAngularDomain::Scalar,
			EPCGExComparison::EqualOrGreater, Tolerance));

	return true;
}

// =============================================================================
// Practical Usage Scenarios
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDotLogicScenarioTest,
	"PCGEx.Unit.Filters.DotLogic.Scenarios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDotLogicScenarioTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Scenario 1: Filter points facing forward (within 45 degrees of +X)
	// 45 degrees = dot ~0.707
	{
		FVector Forward(1, 0, 0);
		FVector PointDir1(1, 0.5, 0);    // ~27 degrees off
		FVector PointDir2(0.5, 1, 0);    // ~63 degrees off

		double Dot1 = DotCompareLogic::ComputeDot(Forward, PointDir1);
		double Dot2 = DotCompareLogic::ComputeDot(Forward, PointDir2);

		const double Threshold45 = DotCompareLogic::DegreesToDot(45.0);

		TestTrue(TEXT("Scenario: Point1 within 45 degrees"),
			DotCompareLogic::Test(Dot1, Threshold45, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance));

		TestFalse(TEXT("Scenario: Point2 outside 45 degrees"),
			DotCompareLogic::Test(Dot2, Threshold45, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance));
	}

	// Scenario 2: Filter normals that are roughly vertical (within 30 degrees of Up)
	// Using SCALAR domain for intuitive threshold behavior
	{
		FVector Up(0, 0, 1);
		FVector Normal1(0, 0.2, 0.98);   // Slight tilt (~11.5 degrees)
		FVector Normal2(0, 0.6, 0.8);    // More tilted (~37 degrees)

		double Dot1 = DotCompareLogic::ComputeDot(Up, Normal1);
		double Dot2 = DotCompareLogic::ComputeDot(Up, Normal2);

		// In scalar domain, threshold is the dot product directly
		// cos(30°) ≈ 0.866 - angles within 30° have dot >= 0.866
		const double Threshold30Scalar = DotCompareLogic::DegreesToDot(30.0);

		TestTrue(TEXT("Scenario: Normal1 within 30 degrees of Up"),
			DotCompareLogic::Test(Dot1, Threshold30Scalar, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, 0.01));

		TestFalse(TEXT("Scenario: Normal2 outside 30 degrees of Up"),
			DotCompareLogic::Test(Dot2, Threshold30Scalar, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, 0.01));
	}

	// Scenario 3: Bidirectional check (using unsigned)
	// Find points roughly aligned with X axis (either +X or -X)
	{
		FVector XAxis(1, 0, 0);
		FVector PointA(1, 0, 0);      // +X
		FVector PointB(-1, 0, 0);     // -X
		FVector PointC(0, 1, 0);      // Y axis (perpendicular)

		double DotA = DotCompareLogic::ComputeDot(XAxis, PointA);
		double DotB = DotCompareLogic::ComputeDot(XAxis, PointB);
		double DotC = DotCompareLogic::ComputeDot(XAxis, PointC);

		const double Threshold = 0.9;

		// Without unsigned, only +X passes
		TestTrue(TEXT("Scenario: +X passes signed check"),
			DotCompareLogic::Test(DotA, Threshold, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance, false));
		TestFalse(TEXT("Scenario: -X fails signed check"),
			DotCompareLogic::Test(DotB, Threshold, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance, false));

		// With unsigned, both +X and -X pass
		TestTrue(TEXT("Scenario: +X passes unsigned check"),
			DotCompareLogic::Test(DotA, Threshold, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance, true));
		TestTrue(TEXT("Scenario: -X passes unsigned check"),
			DotCompareLogic::Test(DotB, Threshold, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance, true));

		// Y axis still fails
		TestFalse(TEXT("Scenario: Y fails unsigned check"),
			DotCompareLogic::Test(DotC, Threshold, EPCGExAngularDomain::Scalar,
				EPCGExComparison::EqualOrGreater, Tolerance, true));
	}

	return true;
}
