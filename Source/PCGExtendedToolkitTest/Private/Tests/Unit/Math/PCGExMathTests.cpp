// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMath Unit Tests
 *
 * Tests core math utilities from PCGExMath namespace:
 * - Tile: Value wrapping within a range
 * - Remap: Linear interpolation between ranges
 * - SanitizeIndex: Index safety with various modes
 * - Distance functions: Manhattan, Chebyshev
 * - Rounding utilities
 *
 * Test naming convention: PCGEx.Unit.Math.<FunctionName>
 *
 * Run these tests:
 * - In Editor: Session Frontend > Automation > Filter "PCGEx.Unit.Math"
 * - Command line: -ExecCmds="Automation RunTests PCGEx.Unit.Math"
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExMath.h"

// =============================================================================
// Tile Function Tests
// =============================================================================

/**
 * Test PCGExMath::Tile with integer values
 *
 * Tile wraps values to stay within [Min, Max] range, cycling like modulo
 * but handling negative values correctly.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathTileIntTest,
	"PCGEx.Unit.Math.Tile.Integer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathTileIntTest::RunTest(const FString& Parameters)
{
	// Basic tiling within range - value already in range should stay unchanged
	TestEqual(TEXT("Tile(1, 0, 3) = 1 (in range)"), PCGExMath::Tile(1, 0, 3), 1);
	TestEqual(TEXT("Tile(0, 0, 3) = 0 (at min)"), PCGExMath::Tile(0, 0, 3), 0);
	TestEqual(TEXT("Tile(3, 0, 3) = 3 (at max)"), PCGExMath::Tile(3, 0, 3), 3);

	// Positive overflow - should wrap back to beginning
	TestEqual(TEXT("Tile(4, 0, 3) = 0 (wrap once)"), PCGExMath::Tile(4, 0, 3), 0);
	TestEqual(TEXT("Tile(5, 0, 3) = 1 (wrap once + 1)"), PCGExMath::Tile(5, 0, 3), 1);
	TestEqual(TEXT("Tile(8, 0, 3) = 0 (wrap twice)"), PCGExMath::Tile(8, 0, 3), 0);

	// Negative values - should wrap from the end
	TestEqual(TEXT("Tile(-1, 0, 3) = 3 (wrap back)"), PCGExMath::Tile(-1, 0, 3), 3);
	TestEqual(TEXT("Tile(-2, 0, 3) = 2"), PCGExMath::Tile(-2, 0, 3), 2);
	TestEqual(TEXT("Tile(-4, 0, 3) = 0"), PCGExMath::Tile(-4, 0, 3), 0);

	// Non-zero minimum range
	TestEqual(TEXT("Tile(15, 10, 14) = 10 (custom min)"), PCGExMath::Tile(15, 10, 14), 10);
	TestEqual(TEXT("Tile(9, 10, 14) = 14 (below min)"), PCGExMath::Tile(9, 10, 14), 14);

	return true;
}

/**
 * Test PCGExMath::Tile with floating point values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathTileFloatTest,
	"PCGEx.Unit.Math.Tile.Float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathTileFloatTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Basic tiling
	TestTrue(TEXT("Tile(0.5, 0.0, 1.0) = 0.5"),
	         FMath::IsNearlyEqual(PCGExMath::Tile(0.5, 0.0, 1.0), 0.5, Tolerance));

	// Positive overflow
	TestTrue(TEXT("Tile(1.25, 0.0, 1.0) ~ 0.25"),
	         FMath::IsNearlyEqual(PCGExMath::Tile(1.25, 0.0, 1.0), 0.25, Tolerance));

	// Negative values
	TestTrue(TEXT("Tile(-0.25, 0.0, 1.0) ~ 0.75"),
	         FMath::IsNearlyEqual(PCGExMath::Tile(-0.25, 0.0, 1.0), 0.75, Tolerance));

	return true;
}

// =============================================================================
// Remap Function Tests
// =============================================================================

/**
 * Test PCGExMath::Remap for linear range conversion
 *
 * Remap converts a value from one range to another linearly:
 * Remap(value, inMin, inMax, outMin, outMax)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathRemapTest,
	"PCGEx.Unit.Math.Remap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathRemapTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Identity remap (same range)
	TestTrue(TEXT("Remap(5, 0, 10, 0, 10) = 5"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(5.0, 0.0, 10.0, 0.0, 10.0), 5.0, Tolerance));

	// Normalize to 0-1
	TestTrue(TEXT("Remap(5, 0, 10) = 0.5 (to 0-1)"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(5.0, 0.0, 10.0), 0.5, Tolerance));
	TestTrue(TEXT("Remap(0, 0, 10) = 0.0"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(0.0, 0.0, 10.0), 0.0, Tolerance));
	TestTrue(TEXT("Remap(10, 0, 10) = 1.0"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(10.0, 0.0, 10.0), 1.0, Tolerance));

	// Scale up
	TestTrue(TEXT("Remap(0.5, 0, 1, 0, 100) = 50"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(0.5, 0.0, 1.0, 0.0, 100.0), 50.0, Tolerance));

	// Inverse range (flip)
	TestTrue(TEXT("Remap(0, 0, 10, 10, 0) = 10"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(0.0, 0.0, 10.0, 10.0, 0.0), 10.0, Tolerance));
	TestTrue(TEXT("Remap(10, 0, 10, 10, 0) = 0"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(10.0, 0.0, 10.0, 10.0, 0.0), 0.0, Tolerance));

	// Extrapolation (value outside input range)
	TestTrue(TEXT("Remap(15, 0, 10, 0, 100) = 150 (extrapolate)"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(15.0, 0.0, 10.0, 0.0, 100.0), 150.0, Tolerance));
	TestTrue(TEXT("Remap(-5, 0, 10, 0, 100) = -50 (extrapolate negative)"),
	         FMath::IsNearlyEqual(PCGExMath::Remap(-5.0, 0.0, 10.0, 0.0, 100.0), -50.0, Tolerance));

	return true;
}

// =============================================================================
// SanitizeIndex Tests
// =============================================================================

/**
 * Test PCGExMath::SanitizeIndex with Ignore mode
 *
 * Ignore mode: Returns -1 for out-of-bounds indices
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSanitizeIndexIgnoreTest,
	"PCGEx.Unit.Math.SanitizeIndex.Ignore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSanitizeIndexIgnoreTest::RunTest(const FString& Parameters)
{
	const int32 MaxIndex = 9; // Array of 10 elements (0-9)

	// Valid indices pass through
	TestEqual(TEXT("SanitizeIndex(0, 9, Ignore) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(0, MaxIndex), 0);
	TestEqual(TEXT("SanitizeIndex(5, 9, Ignore) = 5"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(5, MaxIndex), 5);
	TestEqual(TEXT("SanitizeIndex(9, 9, Ignore) = 9"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(9, MaxIndex), 9);

	// Out of bounds returns -1
	TestEqual(TEXT("SanitizeIndex(-1, 9, Ignore) = -1"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(-1, MaxIndex), -1);
	TestEqual(TEXT("SanitizeIndex(10, 9, Ignore) = -1"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(10, MaxIndex), -1);
	TestEqual(TEXT("SanitizeIndex(100, 9, Ignore) = -1"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(100, MaxIndex), -1);

	return true;
}

/**
 * Test PCGExMath::SanitizeIndex with Clamp mode
 *
 * Clamp mode: Clamps out-of-bounds indices to valid range
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSanitizeIndexClampTest,
	"PCGEx.Unit.Math.SanitizeIndex.Clamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSanitizeIndexClampTest::RunTest(const FString& Parameters)
{
	const int32 MaxIndex = 9;

	// Valid indices pass through
	TestEqual(TEXT("SanitizeIndex(5, 9, Clamp) = 5"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(5, MaxIndex), 5);

	// Clamp to bounds
	TestEqual(TEXT("SanitizeIndex(-1, 9, Clamp) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(-1, MaxIndex), 0);
	TestEqual(TEXT("SanitizeIndex(-100, 9, Clamp) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(-100, MaxIndex), 0);
	TestEqual(TEXT("SanitizeIndex(10, 9, Clamp) = 9"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(10, MaxIndex), 9);
	TestEqual(TEXT("SanitizeIndex(100, 9, Clamp) = 9"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(100, MaxIndex), 9);

	return true;
}

/**
 * Test PCGExMath::SanitizeIndex with Tile mode
 *
 * Tile mode: Wraps indices using modulo arithmetic
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSanitizeIndexTileTest,
	"PCGEx.Unit.Math.SanitizeIndex.Tile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSanitizeIndexTileTest::RunTest(const FString& Parameters)
{
	const int32 MaxIndex = 9; // 10 elements

	// Valid indices pass through
	TestEqual(TEXT("SanitizeIndex(5, 9, Tile) = 5"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(5, MaxIndex), 5);

	// Positive overflow wraps
	TestEqual(TEXT("SanitizeIndex(10, 9, Tile) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(10, MaxIndex), 0);
	TestEqual(TEXT("SanitizeIndex(11, 9, Tile) = 1"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(11, MaxIndex), 1);
	TestEqual(TEXT("SanitizeIndex(20, 9, Tile) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(20, MaxIndex), 0);

	// Negative wraps from end
	TestEqual(TEXT("SanitizeIndex(-1, 9, Tile) = 9"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(-1, MaxIndex), 9);
	TestEqual(TEXT("SanitizeIndex(-10, 9, Tile) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(-10, MaxIndex), 0);

	return true;
}

/**
 * Test PCGExMath::SanitizeIndex with Yoyo mode
 *
 * Yoyo mode: Bounces back and forth like a ping-pong
 * Sequence for MaxIndex=3: 0,1,2,3,2,1,0,1,2,3,...
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSanitizeIndexYoyoTest,
	"PCGEx.Unit.Math.SanitizeIndex.Yoyo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSanitizeIndexYoyoTest::RunTest(const FString& Parameters)
{
	const int32 MaxIndex = 3; // 4 elements: 0,1,2,3

	// Valid indices pass through
	TestEqual(TEXT("SanitizeIndex(0, 3, Yoyo) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(0, MaxIndex), 0);
	TestEqual(TEXT("SanitizeIndex(3, 3, Yoyo) = 3"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(3, MaxIndex), 3);

	// Bounce back after max
	TestEqual(TEXT("SanitizeIndex(4, 3, Yoyo) = 2"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(4, MaxIndex), 2);
	TestEqual(TEXT("SanitizeIndex(5, 3, Yoyo) = 1"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(5, MaxIndex), 1);
	TestEqual(TEXT("SanitizeIndex(6, 3, Yoyo) = 0"),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(6, MaxIndex), 0);

	return true;
}

/**
 * Test runtime dispatch version of SanitizeIndex
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSanitizeIndexRuntimeTest,
	"PCGEx.Unit.Math.SanitizeIndex.RuntimeDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSanitizeIndexRuntimeTest::RunTest(const FString& Parameters)
{
	const int32 MaxIndex = 5;

	// Test runtime dispatch matches compile-time behavior
	TestEqual(TEXT("Runtime Ignore matches compile-time"),
	          PCGExMath::SanitizeIndex(10, MaxIndex, EPCGExIndexSafety::Ignore),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Ignore>(10, MaxIndex));

	TestEqual(TEXT("Runtime Clamp matches compile-time"),
	          PCGExMath::SanitizeIndex(10, MaxIndex, EPCGExIndexSafety::Clamp),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Clamp>(10, MaxIndex));

	TestEqual(TEXT("Runtime Tile matches compile-time"),
	          PCGExMath::SanitizeIndex(10, MaxIndex, EPCGExIndexSafety::Tile),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Tile>(10, MaxIndex));

	TestEqual(TEXT("Runtime Yoyo matches compile-time"),
	          PCGExMath::SanitizeIndex(10, MaxIndex, EPCGExIndexSafety::Yoyo),
	          PCGExMath::SanitizeIndex<int32, EPCGExIndexSafety::Yoyo>(10, MaxIndex));

	return true;
}

// =============================================================================
// Distance Function Tests
// =============================================================================

/**
 * Test Manhattan (L1) distance calculation
 *
 * Manhattan distance = |x1-x2| + |y1-y2| + |z1-z2|
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathDistanceManhattanTest,
	"PCGEx.Unit.Math.Distance.Manhattan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathDistanceManhattanTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Same point
	TestTrue(TEXT("Manhattan(origin, origin) = 0"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceManhattan(FVector::ZeroVector, FVector::ZeroVector),
		         0.0, Tolerance));

	// Axis-aligned distance
	TestTrue(TEXT("Manhattan((0,0,0), (5,0,0)) = 5"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceManhattan(FVector::ZeroVector, FVector(5, 0, 0)),
		         5.0, Tolerance));

	// Multi-axis
	TestTrue(TEXT("Manhattan((0,0,0), (3,4,0)) = 7"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceManhattan(FVector::ZeroVector, FVector(3, 4, 0)),
		         7.0, Tolerance));

	// 3D
	TestTrue(TEXT("Manhattan((1,2,3), (4,6,8)) = 3+4+5 = 12"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceManhattan(FVector(1, 2, 3), FVector(4, 6, 8)),
		         12.0, Tolerance));

	// 2D version
	TestTrue(TEXT("Manhattan2D((0,0), (3,4)) = 7"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceManhattan(FVector2D::ZeroVector, FVector2D(3, 4)),
		         7.0, Tolerance));

	return true;
}

/**
 * Test Chebyshev (L∞) distance calculation
 *
 * Chebyshev distance = max(|x1-x2|, |y1-y2|, |z1-z2|)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathDistanceChebyshevTest,
	"PCGEx.Unit.Math.Distance.Chebyshev",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathDistanceChebyshevTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Same point
	TestTrue(TEXT("Chebyshev(origin, origin) = 0"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceChebyshev(FVector::ZeroVector, FVector::ZeroVector),
		         0.0, Tolerance));

	// Axis-aligned
	TestTrue(TEXT("Chebyshev((0,0,0), (5,0,0)) = 5"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceChebyshev(FVector::ZeroVector, FVector(5, 0, 0)),
		         5.0, Tolerance));

	// Multi-axis - takes maximum
	TestTrue(TEXT("Chebyshev((0,0,0), (3,4,0)) = 4 (max)"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceChebyshev(FVector::ZeroVector, FVector(3, 4, 0)),
		         4.0, Tolerance));

	// 3D
	TestTrue(TEXT("Chebyshev((1,2,3), (4,6,8)) = max(3,4,5) = 5"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceChebyshev(FVector(1, 2, 3), FVector(4, 6, 8)),
		         5.0, Tolerance));

	// 2D version
	TestTrue(TEXT("Chebyshev2D((0,0), (3,4)) = 4"),
	         FMath::IsNearlyEqual(
		         PCGExMath::DistanceChebyshev(FVector2D::ZeroVector, FVector2D(3, 4)),
		         4.0, Tolerance));

	return true;
}

// =============================================================================
// Rounding Tests
// =============================================================================

/**
 * Test Snap function for grid alignment
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSnapTest,
	"PCGEx.Unit.Math.Snap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSnapTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	double Value;

	// Snap to 10-unit grid
	Value = 23.0;
	PCGExMath::Snap(Value, 10.0);
	TestTrue(TEXT("Snap(23, 10) = 20"), FMath::IsNearlyEqual(Value, 20.0, Tolerance));

	Value = 27.0;
	PCGExMath::Snap(Value, 10.0);
	TestTrue(TEXT("Snap(27, 10) = 30"), FMath::IsNearlyEqual(Value, 30.0, Tolerance));

	// Snap to 0.5 grid
	Value = 1.3;
	PCGExMath::Snap(Value, 0.5);
	TestTrue(TEXT("Snap(1.3, 0.5) = 1.5"), FMath::IsNearlyEqual(Value, 1.5, Tolerance));

	// Zero step (no-op)
	Value = 1.3;
	PCGExMath::Snap(Value, 0.0);
	TestTrue(TEXT("Snap(1.3, 0) = 1.3 (no change)"), FMath::IsNearlyEqual(Value, 1.3, Tolerance));

	return true;
}

/**
 * Test Round10 for single decimal precision
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathRound10Test,
	"PCGEx.Unit.Math.Round10",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathRound10Test::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01; // Allow for floating point precision

	// Scalar rounding
	TestTrue(TEXT("Round10(1.234) = 1.2"),
	         FMath::IsNearlyEqual(PCGExMath::Round10(1.234f), 1.2, Tolerance));
	TestTrue(TEXT("Round10(1.256) = 1.3"),
	         FMath::IsNearlyEqual(PCGExMath::Round10(1.256f), 1.3, Tolerance));
	TestTrue(TEXT("Round10(1.25) = 1.3 (round half up)"),
	         FMath::IsNearlyEqual(PCGExMath::Round10(1.25f), 1.3, Tolerance));

	// Vector rounding
	FVector Result = PCGExMath::Round10(FVector(1.234, 2.567, 3.891));
	TestTrue(TEXT("Round10(Vector).X = 1.2"),
	         FMath::IsNearlyEqual(Result.X, 1.2, Tolerance));
	TestTrue(TEXT("Round10(Vector).Y = 2.6"),
	         FMath::IsNearlyEqual(Result.Y, 2.6, Tolerance));
	TestTrue(TEXT("Round10(Vector).Z = 3.9"),
	         FMath::IsNearlyEqual(Result.Z, 3.9, Tolerance));

	return true;
}

// =============================================================================
// Utility Function Tests
// =============================================================================

/**
 * Test DegreesToDot conversion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathDegreesToDotTest,
	"PCGEx.Unit.Math.DegreesToDot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathDegreesToDotTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// 0 degrees = dot product of 1 (parallel)
	TestTrue(TEXT("DegreesToDot(0) = 1.0"),
	         FMath::IsNearlyEqual(PCGExMath::DegreesToDot(0.0), 1.0, Tolerance));

	// 90 degrees = dot product of 0 (perpendicular)
	TestTrue(TEXT("DegreesToDot(90) = 0.0"),
	         FMath::IsNearlyEqual(PCGExMath::DegreesToDot(90.0), 0.0, Tolerance));

	// 180 degrees = dot product of -1 (opposite)
	TestTrue(TEXT("DegreesToDot(180) = -1.0"),
	         FMath::IsNearlyEqual(PCGExMath::DegreesToDot(180.0), -1.0, Tolerance));

	// 45 degrees
	TestTrue(TEXT("DegreesToDot(45) ~ 0.707"),
	         FMath::IsNearlyEqual(PCGExMath::DegreesToDot(45.0), UE_DOUBLE_INV_SQRT_2, 0.001));

	return true;
}

/**
 * Test SignPlus and SignMinus functions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSignTest,
	"PCGEx.Unit.Math.Sign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSignTest::RunTest(const FString& Parameters)
{
	// SignPlus: Returns sign, but 0 becomes +1
	TestEqual(TEXT("SignPlus(5) = 1"), PCGExMath::SignPlus(5), 1);
	TestEqual(TEXT("SignPlus(-5) = -1"), PCGExMath::SignPlus(-5), -1);
	TestEqual(TEXT("SignPlus(0) = 1 (zero becomes positive)"), PCGExMath::SignPlus(0), 1);

	// SignMinus: Returns sign, but 0 becomes -1
	TestEqual(TEXT("SignMinus(5) = 1"), PCGExMath::SignMinus(5), 1);
	TestEqual(TEXT("SignMinus(-5) = -1"), PCGExMath::SignMinus(-5), -1);
	TestEqual(TEXT("SignMinus(0) = -1 (zero becomes negative)"), PCGExMath::SignMinus(0), -1);

	return true;
}
