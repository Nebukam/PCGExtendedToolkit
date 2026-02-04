// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMath Utility Tests
 *
 * Tests additional math utilities from PCGExMath namespace:
 * - TruncateDbl: Value truncation modes
 * - FClosestPosition: Distance tracking helper
 * - FSegment: Line segment utilities
 * - GetPerpendicularDistance: Point-to-line distance
 * - GetMinMax: Array min/max extraction
 *
 * Test naming convention: PCGEx.Unit.Math.<Category>.<Function>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExMath.h"

// =============================================================================
// TruncateDbl Tests
// =============================================================================

/**
 * Test PCGExMath::TruncateDbl with different truncation modes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathTruncateDblTest,
	"PCGEx.Unit.Math.TruncateDbl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathTruncateDblTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;
	const double TestValue = 3.7;
	const double NegativeValue = -3.7;

	// None mode - no truncation
	TestTrue(TEXT("TruncateDbl(3.7, None) = 3.7"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(TestValue, EPCGExTruncateMode::None), 3.7, Tolerance));

	// Round mode
	TestTrue(TEXT("TruncateDbl(3.7, Round) = 4.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(TestValue, EPCGExTruncateMode::Round), 4.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(3.2, Round) = 3.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(3.2, EPCGExTruncateMode::Round), 3.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(-3.7, Round) = -4.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(NegativeValue, EPCGExTruncateMode::Round), -4.0, Tolerance));

	// Ceil mode
	TestTrue(TEXT("TruncateDbl(3.7, Ceil) = 4.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(TestValue, EPCGExTruncateMode::Ceil), 4.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(3.1, Ceil) = 4.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(3.1, EPCGExTruncateMode::Ceil), 4.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(-3.7, Ceil) = -3.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(NegativeValue, EPCGExTruncateMode::Ceil), -3.0, Tolerance));

	// Floor mode
	TestTrue(TEXT("TruncateDbl(3.7, Floor) = 3.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(TestValue, EPCGExTruncateMode::Floor), 3.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(3.9, Floor) = 3.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(3.9, EPCGExTruncateMode::Floor), 3.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(-3.7, Floor) = -4.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(NegativeValue, EPCGExTruncateMode::Floor), -4.0, Tolerance));

	// Whole numbers should remain unchanged
	TestTrue(TEXT("TruncateDbl(5.0, Round) = 5.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(5.0, EPCGExTruncateMode::Round), 5.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(5.0, Ceil) = 5.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(5.0, EPCGExTruncateMode::Ceil), 5.0, Tolerance));
	TestTrue(TEXT("TruncateDbl(5.0, Floor) = 5.0"),
	         FMath::IsNearlyEqual(PCGExMath::TruncateDbl(5.0, EPCGExTruncateMode::Floor), 5.0, Tolerance));

	return true;
}

// =============================================================================
// FClosestPosition Tests
// =============================================================================

/**
 * Test FClosestPosition construction and basic operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathClosestPositionConstructionTest,
	"PCGEx.Unit.Math.ClosestPosition.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathClosestPositionConstructionTest::RunTest(const FString& Parameters)
{
	// Default construction
	{
		PCGExMath::FClosestPosition Closest;
		TestFalse(TEXT("Default construction is not valid"), Closest.bValid);
		TestEqual(TEXT("Default index is -1"), Closest.Index, -1);
		TestEqual(TEXT("Default dist squared is MAX_dbl"), Closest.DistSquared, MAX_dbl);
	}

	// Construction with origin
	{
		FVector Origin(10, 20, 30);
		PCGExMath::FClosestPosition Closest(Origin);
		TestFalse(TEXT("Origin-only construction is not valid"), Closest.bValid);
		TestTrue(TEXT("Origin is set correctly"), Closest.Origin.Equals(Origin, KINDA_SMALL_NUMBER));
	}

	// Construction with origin and location
	{
		FVector Origin(0, 0, 0);
		FVector Location(3, 4, 0);
		PCGExMath::FClosestPosition Closest(Origin, Location);
		TestTrue(TEXT("Origin+Location construction is valid"), Closest.bValid);
		TestTrue(TEXT("DistSquared is 25 (3-4-5 triangle)"),
		         FMath::IsNearlyEqual(Closest.DistSquared, 25.0, KINDA_SMALL_NUMBER));
	}

	// Construction with origin, location, and index
	{
		FVector Origin(0, 0, 0);
		FVector Location(10, 0, 0);
		PCGExMath::FClosestPosition Closest(Origin, Location, 42);
		TestTrue(TEXT("Full construction is valid"), Closest.bValid);
		TestEqual(TEXT("Index is set correctly"), Closest.Index, 42);
		TestTrue(TEXT("DistSquared is 100"),
		         FMath::IsNearlyEqual(Closest.DistSquared, 100.0, KINDA_SMALL_NUMBER));
	}

	return true;
}

/**
 * Test FClosestPosition Update method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathClosestPositionUpdateTest,
	"PCGEx.Unit.Math.ClosestPosition.Update",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathClosestPositionUpdateTest::RunTest(const FString& Parameters)
{
	FVector Origin(0, 0, 0);
	PCGExMath::FClosestPosition Closest(Origin);

	// First update should always succeed (any distance is less than MAX_dbl)
	FVector FirstLocation(100, 0, 0);
	bool Updated = Closest.Update(FirstLocation);
	TestTrue(TEXT("First update succeeds"), Updated);
	TestTrue(TEXT("Now valid after first update"), Closest.bValid);
	TestTrue(TEXT("Location updated to first point"),
	         Closest.Location.Equals(FirstLocation, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("DistSquared is 10000"),
	         FMath::IsNearlyEqual(Closest.DistSquared, 10000.0, KINDA_SMALL_NUMBER));

	// Update with closer point should succeed
	FVector CloserLocation(10, 0, 0);
	Updated = Closest.Update(CloserLocation);
	TestTrue(TEXT("Closer update succeeds"), Updated);
	TestTrue(TEXT("Location updated to closer point"),
	         Closest.Location.Equals(CloserLocation, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("DistSquared is now 100"),
	         FMath::IsNearlyEqual(Closest.DistSquared, 100.0, KINDA_SMALL_NUMBER));

	// Update with farther point should fail
	FVector FartherLocation(50, 0, 0);
	Updated = Closest.Update(FartherLocation);
	TestFalse(TEXT("Farther update fails"), Updated);
	TestTrue(TEXT("Location remains at closer point"),
	         Closest.Location.Equals(CloserLocation, KINDA_SMALL_NUMBER));

	// Update with index
	FVector EvenCloser(5, 0, 0);
	Updated = Closest.Update(EvenCloser, 99);
	TestTrue(TEXT("Even closer update with index succeeds"), Updated);
	TestEqual(TEXT("Index updated to 99"), Closest.Index, 99);

	return true;
}

/**
 * Test FClosestPosition comparison operators
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathClosestPositionComparisonTest,
	"PCGEx.Unit.Math.ClosestPosition.Comparison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathClosestPositionComparisonTest::RunTest(const FString& Parameters)
{
	FVector Origin(0, 0, 0);

	PCGExMath::FClosestPosition Near(Origin, FVector(10, 0, 0));   // DistSq = 100
	PCGExMath::FClosestPosition Far(Origin, FVector(100, 0, 0));   // DistSq = 10000

	TestTrue(TEXT("Near < Far"), Near < Far);
	TestFalse(TEXT("Far < Near is false"), Far < Near);
	TestTrue(TEXT("Far > Near"), Far > Near);
	TestFalse(TEXT("Near > Far is false"), Near > Far);

	// Bool operator
	TestTrue(TEXT("Valid FClosestPosition is truthy"), static_cast<bool>(Near));

	PCGExMath::FClosestPosition Invalid;
	TestFalse(TEXT("Invalid FClosestPosition is falsy"), static_cast<bool>(Invalid));

	// Double operator (returns DistSquared)
	TestTrue(TEXT("Double cast returns DistSquared"),
	         FMath::IsNearlyEqual(static_cast<double>(Near), 100.0, KINDA_SMALL_NUMBER));

	// Vector operator (returns Location)
	FVector NearAsVector = Near;
	TestTrue(TEXT("Vector cast returns Location"),
	         NearAsVector.Equals(FVector(10, 0, 0), KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// FSegment Tests
// =============================================================================

/**
 * Test FSegment construction and basic properties
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSegmentConstructionTest,
	"PCGEx.Unit.Math.Segment.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSegmentConstructionTest::RunTest(const FString& Parameters)
{
	// Horizontal segment
	{
		FVector A(0, 0, 0);
		FVector B(100, 0, 0);
		PCGExMath::FSegment Segment(A, B);

		TestTrue(TEXT("A is set correctly"), Segment.A.Equals(A, KINDA_SMALL_NUMBER));
		TestTrue(TEXT("B is set correctly"), Segment.B.Equals(B, KINDA_SMALL_NUMBER));

		// Direction should be normalized
		TestTrue(TEXT("Direction is (1,0,0)"),
		         Segment.Direction.Equals(FVector(1, 0, 0), KINDA_SMALL_NUMBER));

		// Bounds should span from A to B
		// Note: For axis-aligned segments, bounds are degenerate (zero volume),
		// so we check Min/Max directly rather than using IsInside
		TestTrue(TEXT("Bounds Min <= A"),
		         Segment.Bounds.Min.X <= A.X && Segment.Bounds.Min.Y <= A.Y && Segment.Bounds.Min.Z <= A.Z);
		TestTrue(TEXT("Bounds Max >= B"),
		         Segment.Bounds.Max.X >= B.X && Segment.Bounds.Max.Y >= B.Y && Segment.Bounds.Max.Z >= B.Z);
	}

	// Diagonal segment
	{
		FVector A(0, 0, 0);
		FVector B(10, 10, 10);
		PCGExMath::FSegment Segment(A, B);

		FVector ExpectedDir = (B - A).GetSafeNormal();
		TestTrue(TEXT("Direction is normalized diagonal"),
		         Segment.Direction.Equals(ExpectedDir, KINDA_SMALL_NUMBER));
	}

	// Segment with expansion
	{
		FVector A(0, 0, 0);
		FVector B(10, 0, 0);
		const double Expansion = 5.0;
		PCGExMath::FSegment Segment(A, B, Expansion);

		// Bounds should be expanded
		TestTrue(TEXT("Expanded bounds Min.Y < 0"), Segment.Bounds.Min.Y < 0);
		TestTrue(TEXT("Expanded bounds Max.Y > 0"), Segment.Bounds.Max.Y > 0);
	}

	return true;
}

/**
 * Test FSegment Lerp method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSegmentLerpTest,
	"PCGEx.Unit.Math.Segment.Lerp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSegmentLerpTest::RunTest(const FString& Parameters)
{
	FVector A(0, 0, 0);
	FVector B(100, 100, 100);
	PCGExMath::FSegment Segment(A, B);

	// Lerp at 0 = A
	FVector At0 = Segment.Lerp(0.0);
	TestTrue(TEXT("Lerp(0) = A"), At0.Equals(A, KINDA_SMALL_NUMBER));

	// Lerp at 1 = B
	FVector At1 = Segment.Lerp(1.0);
	TestTrue(TEXT("Lerp(1) = B"), At1.Equals(B, KINDA_SMALL_NUMBER));

	// Lerp at 0.5 = midpoint
	FVector AtHalf = Segment.Lerp(0.5);
	FVector Midpoint(50, 50, 50);
	TestTrue(TEXT("Lerp(0.5) = midpoint"), AtHalf.Equals(Midpoint, KINDA_SMALL_NUMBER));

	// Lerp at 0.25
	FVector AtQuarter = Segment.Lerp(0.25);
	FVector Quarter(25, 25, 25);
	TestTrue(TEXT("Lerp(0.25) = quarter"), AtQuarter.Equals(Quarter, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FSegment Dot method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSegmentDotTest,
	"PCGEx.Unit.Math.Segment.Dot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSegmentDotTest::RunTest(const FString& Parameters)
{
	// Segment along X axis
	PCGExMath::FSegment SegmentX(FVector::ZeroVector, FVector(100, 0, 0));

	// Dot with same direction
	TestTrue(TEXT("Dot with X forward = 1"),
	         FMath::IsNearlyEqual(SegmentX.Dot(FVector(1, 0, 0)), 1.0, KINDA_SMALL_NUMBER));

	// Dot with opposite direction
	TestTrue(TEXT("Dot with X backward = -1"),
	         FMath::IsNearlyEqual(SegmentX.Dot(FVector(-1, 0, 0)), -1.0, KINDA_SMALL_NUMBER));

	// Dot with perpendicular direction
	TestTrue(TEXT("Dot with Y = 0"),
	         FMath::IsNearlyEqual(SegmentX.Dot(FVector(0, 1, 0)), 0.0, KINDA_SMALL_NUMBER));

	// Dot with another segment
	PCGExMath::FSegment SegmentY(FVector::ZeroVector, FVector(0, 100, 0));
	TestTrue(TEXT("X segment dot Y segment = 0"),
	         FMath::IsNearlyEqual(SegmentX.Dot(SegmentY), 0.0, KINDA_SMALL_NUMBER));

	PCGExMath::FSegment SegmentXNeg(FVector::ZeroVector, FVector(-100, 0, 0));
	TestTrue(TEXT("X segment dot -X segment = -1"),
	         FMath::IsNearlyEqual(SegmentX.Dot(SegmentXNeg), -1.0, KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// GetPerpendicularDistance Tests
// =============================================================================

/**
 * Test GetPerpendicularDistance - distance from point to line
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetPerpendicularDistanceTest,
	"PCGEx.Unit.Math.GetPerpendicularDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetPerpendicularDistanceTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Point directly above line segment midpoint
	{
		FVector A(0, 0, 0);
		FVector B(10, 0, 0);
		FVector C(5, 5, 0);  // 5 units above midpoint

		double Dist = PCGExMath::GetPerpendicularDistance(A, B, C);
		TestTrue(TEXT("Point 5 units above line = distance 5"),
		         FMath::IsNearlyEqual(Dist, 5.0, Tolerance));
	}

	// Point on the line
	{
		FVector A(0, 0, 0);
		FVector B(10, 0, 0);
		FVector C(5, 0, 0);  // On the line

		double Dist = PCGExMath::GetPerpendicularDistance(A, B, C);
		TestTrue(TEXT("Point on line = distance 0"),
		         FMath::IsNearlyEqual(Dist, 0.0, Tolerance));
	}

	// Classic 3-4-5 triangle perpendicular
	{
		FVector A(0, 0, 0);
		FVector B(4, 0, 0);
		FVector C(0, 3, 0);  // Point at height 3

		// The perpendicular from C to line AB has length 3 (since AB is along X)
		double Dist = PCGExMath::GetPerpendicularDistance(A, B, C);
		TestTrue(TEXT("3-4-5 triangle perpendicular distance = 3"),
		         FMath::IsNearlyEqual(Dist, 3.0, Tolerance));
	}

	// Vertical line
	{
		FVector A(0, 0, 0);
		FVector B(0, 0, 10);
		FVector C(5, 0, 5);  // 5 units away from vertical line

		double Dist = PCGExMath::GetPerpendicularDistance(A, B, C);
		TestTrue(TEXT("Point 5 units from vertical line"),
		         FMath::IsNearlyEqual(Dist, 5.0, Tolerance));
	}

	return true;
}

// =============================================================================
// GetMinMax Tests
// =============================================================================

/**
 * Test GetMinMax template function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetMinMaxTest,
	"PCGEx.Unit.Math.GetMinMax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetMinMaxTest::RunTest(const FString& Parameters)
{
	// Integer array
	{
		TArray<int32> Values = {5, 2, 8, 1, 9, 3};
		int32 Min, Max;
		PCGExMath::GetMinMax(Values, Min, Max);

		TestEqual(TEXT("Int array min = 1"), Min, 1);
		TestEqual(TEXT("Int array max = 9"), Max, 9);
	}

	// Float array
	{
		TArray<float> Values = {1.5f, -2.3f, 4.7f, 0.0f};
		float Min, Max;
		PCGExMath::GetMinMax(Values, Min, Max);

		TestTrue(TEXT("Float array min = -2.3"),
		         FMath::IsNearlyEqual(Min, -2.3f, 0.01f));
		TestTrue(TEXT("Float array max = 4.7"),
		         FMath::IsNearlyEqual(Max, 4.7f, 0.01f));
	}

	// Double array
	{
		TArray<double> Values = {100.0, 200.0, 50.0, 150.0};
		double Min, Max;
		PCGExMath::GetMinMax(Values, Min, Max);

		TestTrue(TEXT("Double array min = 50"),
		         FMath::IsNearlyEqual(Min, 50.0, 0.01));
		TestTrue(TEXT("Double array max = 200"),
		         FMath::IsNearlyEqual(Max, 200.0, 0.01));
	}

	// Single element array
	{
		TArray<int32> Values = {42};
		int32 Min, Max;
		PCGExMath::GetMinMax(Values, Min, Max);

		TestEqual(TEXT("Single element min = max = 42"), Min, 42);
		TestEqual(TEXT("Single element max = 42"), Max, 42);
	}

	// All same values
	{
		TArray<int32> Values = {7, 7, 7, 7};
		int32 Min, Max;
		PCGExMath::GetMinMax(Values, Min, Max);

		TestEqual(TEXT("All same values min = 7"), Min, 7);
		TestEqual(TEXT("All same values max = 7"), Max, 7);
	}

	return true;
}

// =============================================================================
// ReverseRange Tests
// =============================================================================

/**
 * Test ReverseRange for reversing portions of arrays
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathReverseRangeTest,
	"PCGEx.Unit.Math.ReverseRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathReverseRangeTest::RunTest(const FString& Parameters)
{
	// Reverse entire array
	{
		TArray<int32> Values = {1, 2, 3, 4, 5};
		PCGExMath::ReverseRange(Values, 0, 4);

		TestEqual(TEXT("Full reverse [0]=5"), Values[0], 5);
		TestEqual(TEXT("Full reverse [1]=4"), Values[1], 4);
		TestEqual(TEXT("Full reverse [2]=3"), Values[2], 3);
		TestEqual(TEXT("Full reverse [3]=2"), Values[3], 2);
		TestEqual(TEXT("Full reverse [4]=1"), Values[4], 1);
	}

	// Reverse partial range
	{
		TArray<int32> Values = {1, 2, 3, 4, 5};
		PCGExMath::ReverseRange(Values, 1, 3);

		TestEqual(TEXT("Partial reverse [0]=1 (unchanged)"), Values[0], 1);
		TestEqual(TEXT("Partial reverse [1]=4"), Values[1], 4);
		TestEqual(TEXT("Partial reverse [2]=3"), Values[2], 3);
		TestEqual(TEXT("Partial reverse [3]=2"), Values[3], 2);
		TestEqual(TEXT("Partial reverse [4]=5 (unchanged)"), Values[4], 5);
	}

	// Single element range (no-op)
	{
		TArray<int32> Values = {1, 2, 3};
		PCGExMath::ReverseRange(Values, 1, 1);

		TestEqual(TEXT("Single element [0]=1"), Values[0], 1);
		TestEqual(TEXT("Single element [1]=2"), Values[1], 2);
		TestEqual(TEXT("Single element [2]=3"), Values[2], 3);
	}

	// Two element range
	{
		TArray<int32> Values = {1, 2, 3};
		PCGExMath::ReverseRange(Values, 0, 1);

		TestEqual(TEXT("Two element [0]=2"), Values[0], 2);
		TestEqual(TEXT("Two element [1]=1"), Values[1], 1);
		TestEqual(TEXT("Two element [2]=3 (unchanged)"), Values[2], 3);
	}

	return true;
}
