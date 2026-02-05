// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExWinding Unit Tests
 *
 * Tests winding order functions from PCGExWinding.h:
 * - IsWinded: Check if winding matches expected direction
 * - FPolygonInfos: Polygon metrics (area, perimeter, winding, compactness)
 * - AngleCCW: Counter-clockwise angle calculation
 *
 * Test naming convention: PCGEx.Unit.Math.Winding.<FunctionName>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExWinding.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// IsWinded Tests
// =============================================================================

/**
 * Test IsWinded with EPCGExWinding enum
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingIsWindedTest,
	"PCGEx.Unit.Math.Winding.IsWinded.Winding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingIsWindedTest::RunTest(const FString& Parameters)
{
	// Clockwise input with Clockwise expected
	TestTrue(TEXT("CW input matches CW expected"),
	         PCGExMath::IsWinded(EPCGExWinding::Clockwise, true));

	// Clockwise input with CCW expected
	TestFalse(TEXT("CW input doesn't match CCW expected"),
	          PCGExMath::IsWinded(EPCGExWinding::CounterClockwise, true));

	// CCW input with CCW expected
	TestTrue(TEXT("CCW input matches CCW expected"),
	         PCGExMath::IsWinded(EPCGExWinding::CounterClockwise, false));

	// CCW input with CW expected
	TestFalse(TEXT("CCW input doesn't match CW expected"),
	          PCGExMath::IsWinded(EPCGExWinding::Clockwise, false));

	return true;
}

/**
 * Test IsWinded with EPCGExWindingMutation enum
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingIsWindedMutationTest,
	"PCGEx.Unit.Math.Winding.IsWinded.WindingMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingIsWindedMutationTest::RunTest(const FString& Parameters)
{
	// Unchanged always returns true
	TestTrue(TEXT("Unchanged + CW = true"),
	         PCGExMath::IsWinded(EPCGExWindingMutation::Unchanged, true));
	TestTrue(TEXT("Unchanged + CCW = true"),
	         PCGExMath::IsWinded(EPCGExWindingMutation::Unchanged, false));

	// Clockwise mutation
	TestTrue(TEXT("CW mutation + CW input = true"),
	         PCGExMath::IsWinded(EPCGExWindingMutation::Clockwise, true));
	TestFalse(TEXT("CW mutation + CCW input = false"),
	          PCGExMath::IsWinded(EPCGExWindingMutation::Clockwise, false));

	// CounterClockwise mutation
	TestTrue(TEXT("CCW mutation + CCW input = true"),
	         PCGExMath::IsWinded(EPCGExWindingMutation::CounterClockwise, false));
	TestFalse(TEXT("CCW mutation + CW input = false"),
	          PCGExMath::IsWinded(EPCGExWindingMutation::CounterClockwise, true));

	return true;
}

// =============================================================================
// AngleCCW Tests
// =============================================================================

/**
 * Test AngleCCW function for 2D vectors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingAngleCCW2DTest,
	"PCGEx.Unit.Math.Winding.AngleCCW.2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingAngleCCW2DTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Same direction - 0 radians
	{
		FVector2D A(1, 0);
		FVector2D B(1, 0);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("Same direction = 0 radians"),
		         FMath::IsNearlyEqual(Angle, 0.0, Tolerance));
	}

	// 90 degrees CCW
	{
		FVector2D A(1, 0);
		FVector2D B(0, 1);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("90 CCW ~ PI/2"),
		         FMath::IsNearlyEqual(Angle, UE_HALF_PI, Tolerance));
	}

	// 180 degrees
	{
		FVector2D A(1, 0);
		FVector2D B(-1, 0);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("180 degrees ~ PI"),
		         FMath::IsNearlyEqual(Angle, UE_PI, Tolerance));
	}

	// 270 degrees CCW (or 90 CW)
	{
		FVector2D A(1, 0);
		FVector2D B(0, -1);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("270 CCW ~ 3*PI/2"),
		         FMath::IsNearlyEqual(Angle, UE_PI * 1.5, Tolerance));
	}

	return true;
}

/**
 * Test AngleCCW function for 3D vectors (using XY components)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingAngleCCW3DTest,
	"PCGEx.Unit.Math.Winding.AngleCCW.3D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingAngleCCW3DTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Using FVector (template uses [0] and [1] which are X and Y)
	{
		FVector A(1, 0, 0);
		FVector B(0, 1, 0);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("3D AngleCCW uses XY: 90 CCW ~ PI/2"),
		         FMath::IsNearlyEqual(Angle, UE_HALF_PI, Tolerance));
	}

	// Z component should be ignored
	{
		FVector A(1, 0, 100);
		FVector B(0, 1, -50);
		double Angle = PCGExMath::AngleCCW(A, B);
		TestTrue(TEXT("Z component ignored"),
		         FMath::IsNearlyEqual(Angle, UE_HALF_PI, Tolerance));
	}

	return true;
}

// =============================================================================
// FPolygonInfos Tests
// =============================================================================

/**
 * Test FPolygonInfos with a simple square
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosSquareTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.Square",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosSquareTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Unit square (CCW winding)
	{
		TArray<FVector2D> Square = {
			FVector2D(0, 0),
			FVector2D(1, 0),
			FVector2D(1, 1),
			FVector2D(0, 1)
		};

		PCGExMath::FPolygonInfos Info(Square);

		// Area should be 1.0
		TestTrue(TEXT("Unit square area ~ 1.0"),
		         FMath::IsNearlyEqual(FMath::Abs(Info.Area), 1.0, Tolerance));

		// Perimeter should be 4.0
		TestTrue(TEXT("Unit square perimeter ~ 4.0"),
		         FMath::IsNearlyEqual(Info.Perimeter, 4.0, Tolerance));

		// Compactness should be meaningful
		TestTrue(TEXT("Square has positive compactness"), Info.Compactness > 0.0);
	}

	// Larger square
	{
		TArray<FVector2D> Square = {
			FVector2D(0, 0),
			FVector2D(10, 0),
			FVector2D(10, 10),
			FVector2D(0, 10)
		};

		PCGExMath::FPolygonInfos Info(Square);

		TestTrue(TEXT("10x10 square area ~ 100"),
		         FMath::IsNearlyEqual(FMath::Abs(Info.Area), 100.0, Tolerance));

		TestTrue(TEXT("10x10 square perimeter ~ 40"),
		         FMath::IsNearlyEqual(Info.Perimeter, 40.0, Tolerance));
	}

	return true;
}

/**
 * Test FPolygonInfos with a triangle
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosTriangleTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosTriangleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Right triangle with legs of length 3 and 4
	{
		TArray<FVector2D> Triangle = {
			FVector2D(0, 0),
			FVector2D(3, 0),
			FVector2D(0, 4)
		};

		PCGExMath::FPolygonInfos Info(Triangle);

		// Area = 0.5 * base * height = 0.5 * 3 * 4 = 6
		TestTrue(TEXT("3-4-5 triangle area ~ 6"),
		         FMath::IsNearlyEqual(FMath::Abs(Info.Area), 6.0, Tolerance));

		// Perimeter = 3 + 4 + 5 = 12
		TestTrue(TEXT("3-4-5 triangle perimeter ~ 12"),
		         FMath::IsNearlyEqual(Info.Perimeter, 12.0, Tolerance));
	}

	// Equilateral triangle
	{
		const double Side = 2.0;
		const double Height = Side * FMath::Sqrt(3.0) / 2.0;

		TArray<FVector2D> Triangle = {
			FVector2D(0, 0),
			FVector2D(Side, 0),
			FVector2D(Side / 2.0, Height)
		};

		PCGExMath::FPolygonInfos Info(Triangle);

		// Area = (sqrt(3)/4) * side^2
		double ExpectedArea = (FMath::Sqrt(3.0) / 4.0) * Side * Side;
		TestTrue(TEXT("Equilateral triangle area"),
		         FMath::IsNearlyEqual(FMath::Abs(Info.Area), ExpectedArea, Tolerance));

		// Perimeter = 3 * side
		TestTrue(TEXT("Equilateral triangle perimeter ~ 6"),
		         FMath::IsNearlyEqual(Info.Perimeter, 3.0 * Side, Tolerance));
	}

	return true;
}

/**
 * Test FPolygonInfos winding detection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosWindingTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.Winding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosWindingTest::RunTest(const FString& Parameters)
{
	// Counter-clockwise square
	{
		TArray<FVector2D> CCWSquare = {
			FVector2D(0, 0),
			FVector2D(1, 0),
			FVector2D(1, 1),
			FVector2D(0, 1)
		};

		PCGExMath::FPolygonInfos Info(CCWSquare);

		// Sign of area indicates winding
		// Positive area = CCW, Negative area = CW (or vice versa depending on convention)
		bool bExpectedCW = Info.bIsClockwise;

		// Reverse the polygon
		TArray<FVector2D> CWSquare = {
			FVector2D(0, 1),
			FVector2D(1, 1),
			FVector2D(1, 0),
			FVector2D(0, 0)
		};

		PCGExMath::FPolygonInfos InfoReversed(CWSquare);

		// Reversed polygon should have opposite winding
		TestTrue(TEXT("Reversed polygon has opposite winding"),
		         Info.bIsClockwise != InfoReversed.bIsClockwise);
	}

	return true;
}

/**
 * Test FPolygonInfos::IsWinded method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosIsWindedTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.IsWinded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosIsWindedTest::RunTest(const FString& Parameters)
{
	// Create a polygon with known winding
	TArray<FVector2D> Polygon = {
		FVector2D(0, 0),
		FVector2D(1, 0),
		FVector2D(1, 1),
		FVector2D(0, 1)
	};

	PCGExMath::FPolygonInfos Info(Polygon);

	if (Info.bIsClockwise)
	{
		TestTrue(TEXT("CW polygon IsWinded(CW) = true"),
		         Info.IsWinded(EPCGExWinding::Clockwise));
		TestFalse(TEXT("CW polygon IsWinded(CCW) = false"),
		          Info.IsWinded(EPCGExWinding::CounterClockwise));
	}
	else
	{
		TestTrue(TEXT("CCW polygon IsWinded(CCW) = true"),
		         Info.IsWinded(EPCGExWinding::CounterClockwise));
		TestFalse(TEXT("CCW polygon IsWinded(CW) = false"),
		          Info.IsWinded(EPCGExWinding::Clockwise));
	}

	return true;
}

/**
 * Test FPolygonInfos compactness calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosCompactnessTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.Compactness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosCompactnessTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Circle has compactness of 1.0 (highest)
	// Square has lower compactness than circle
	// Long thin rectangle has even lower compactness

	// Unit square
	{
		TArray<FVector2D> Square = {
			FVector2D(0, 0),
			FVector2D(1, 0),
			FVector2D(1, 1),
			FVector2D(0, 1)
		};

		PCGExMath::FPolygonInfos SquareInfo(Square);

		// Square compactness = 4*PI*Area / Perimeter^2 = 4*PI*1 / 16 = PI/4 ~ 0.785
		double ExpectedCompactness = UE_PI / 4.0;
		TestTrue(TEXT("Square compactness ~ PI/4"),
		         FMath::IsNearlyEqual(SquareInfo.Compactness, ExpectedCompactness, Tolerance));
	}

	// Long thin rectangle (1x10)
	{
		TArray<FVector2D> Rect = {
			FVector2D(0, 0),
			FVector2D(10, 0),
			FVector2D(10, 1),
			FVector2D(0, 1)
		};

		PCGExMath::FPolygonInfos RectInfo(Rect);

		// Compactness = 4*PI*10 / 22^2 = 40*PI / 484 ~ 0.26
		// Should be less than square's compactness
		TArray<FVector2D> Square = {
			FVector2D(0, 0),
			FVector2D(1, 0),
			FVector2D(1, 1),
			FVector2D(0, 1)
		};
		PCGExMath::FPolygonInfos SquareInfo(Square);

		TestTrue(TEXT("Thin rectangle less compact than square"),
		         RectInfo.Compactness < SquareInfo.Compactness);
	}

	return true;
}

/**
 * Test FPolygonInfos default constructor
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingPolygonInfosDefaultTest,
	"PCGEx.Unit.Math.Winding.PolygonInfos.DefaultConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingPolygonInfosDefaultTest::RunTest(const FString& Parameters)
{
	PCGExMath::FPolygonInfos Info;

	TestTrue(TEXT("Default Area = 0"), FMath::IsNearlyZero(Info.Area));
	TestTrue(TEXT("Default Perimeter = 0"), FMath::IsNearlyZero(Info.Perimeter));
	TestTrue(TEXT("Default Compactness = 0"), FMath::IsNearlyZero(Info.Compactness));
	TestFalse(TEXT("Default bIsClockwise = false"), Info.bIsClockwise);

	return true;
}

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExWinding enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingEnumTest,
	"PCGEx.Unit.Math.Winding.Enums.Winding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Clockwise = 1"), static_cast<uint8>(EPCGExWinding::Clockwise), 1);
	TestEqual(TEXT("CounterClockwise = 2"), static_cast<uint8>(EPCGExWinding::CounterClockwise), 2);

	TestTrue(TEXT("CW != CCW"), EPCGExWinding::Clockwise != EPCGExWinding::CounterClockwise);

	return true;
}

/**
 * Test EPCGExWindingMutation enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExWindingMutationEnumTest,
	"PCGEx.Unit.Math.Winding.Enums.WindingMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExWindingMutationEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Unchanged = 0"), static_cast<uint8>(EPCGExWindingMutation::Unchanged), 0);
	TestEqual(TEXT("Clockwise = 1"), static_cast<uint8>(EPCGExWindingMutation::Clockwise), 1);
	TestEqual(TEXT("CounterClockwise = 2"), static_cast<uint8>(EPCGExWindingMutation::CounterClockwise), 2);

	return true;
}
