// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMathMean Unit Tests
 *
 * Tests statistical functions from PCGExMathMean.h:
 * - GetAverage: Arithmetic mean of array values
 * - GetMedian: Median using quickselect algorithm
 * - QuickSelectPartition/QuickSelect: Internal quickselect helpers
 *
 * Test naming convention: PCGEx.Unit.Math.Mean.<FunctionName>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExMathMean.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// GetAverage Tests
// =============================================================================

/**
 * Test GetAverage with double values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanAverageDoubleTest,
	"PCGEx.Unit.Math.Mean.GetAverage.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanAverageDoubleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Single value
	{
		TArray<double> Values = {5.0};
		TestTrue(TEXT("Average of single value = value"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 5.0, Tolerance));
	}

	// Two values
	{
		TArray<double> Values = {2.0, 8.0};
		TestTrue(TEXT("Average(2, 8) = 5"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 5.0, Tolerance));
	}

	// Multiple values
	{
		TArray<double> Values = {1.0, 2.0, 3.0, 4.0, 5.0};
		TestTrue(TEXT("Average(1,2,3,4,5) = 3"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 3.0, Tolerance));
	}

	// Negative values
	{
		TArray<double> Values = {-5.0, 5.0};
		TestTrue(TEXT("Average(-5, 5) = 0"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 0.0, Tolerance));
	}

	// All same values
	{
		TArray<double> Values = {7.0, 7.0, 7.0, 7.0};
		TestTrue(TEXT("Average(7,7,7,7) = 7"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 7.0, Tolerance));
	}

	// Large spread
	{
		TArray<double> Values = {0.0, 100.0};
		TestTrue(TEXT("Average(0, 100) = 50"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 50.0, Tolerance));
	}

	return true;
}

/**
 * Test GetAverage with integer values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanAverageIntTest,
	"PCGEx.Unit.Math.Mean.GetAverage.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanAverageIntTest::RunTest(const FString& Parameters)
{
	// Note: Integer average may truncate

	// Simple average
	{
		TArray<int32> Values = {2, 4, 6};
		TestEqual(TEXT("Average(2,4,6) = 4"), PCGExMath::GetAverage(Values), 4);
	}

	// Average with truncation
	{
		TArray<int32> Values = {1, 2};
		// (1+2)/2 = 1.5, truncates to 1 for integer
		TestEqual(TEXT("Average(1,2) = 1 (int truncation)"), PCGExMath::GetAverage(Values), 1);
	}

	// Larger values
	{
		TArray<int32> Values = {10, 20, 30, 40, 50};
		TestEqual(TEXT("Average(10,20,30,40,50) = 30"), PCGExMath::GetAverage(Values), 30);
	}

	return true;
}

/**
 * Test GetAverage with float values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanAverageFloatTest,
	"PCGEx.Unit.Math.Mean.GetAverage.Float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanAverageFloatTest::RunTest(const FString& Parameters)
{
	const float Tolerance = KINDA_SMALL_NUMBER;

	// Decimal average
	{
		TArray<float> Values = {1.5f, 2.5f, 3.5f};
		TestTrue(TEXT("Average(1.5, 2.5, 3.5) = 2.5"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 2.5f, Tolerance));
	}

	// Mixed positive/negative
	{
		TArray<float> Values = {-1.0f, 0.0f, 1.0f};
		TestTrue(TEXT("Average(-1, 0, 1) = 0"),
		         FMath::IsNearlyEqual(PCGExMath::GetAverage(Values), 0.0f, Tolerance));
	}

	return true;
}

// =============================================================================
// GetMedian Tests
// =============================================================================

/**
 * Test GetMedian with odd count
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMedianOddTest,
	"PCGEx.Unit.Math.Mean.GetMedian.OddCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMedianOddTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Single element
	{
		TArray<double> Values = {42.0};
		TestTrue(TEXT("Median of single value = value"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 42.0, Tolerance));
	}

	// Three elements (sorted)
	{
		TArray<double> Values = {1.0, 2.0, 3.0};
		TestTrue(TEXT("Median(1,2,3) = 2"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2.0, Tolerance));
	}

	// Three elements (unsorted)
	{
		TArray<double> Values = {3.0, 1.0, 2.0};
		TestTrue(TEXT("Median(3,1,2) = 2"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2.0, Tolerance));
	}

	// Five elements
	{
		TArray<double> Values = {5.0, 1.0, 3.0, 4.0, 2.0};
		TestTrue(TEXT("Median(5,1,3,4,2) = 3"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 3.0, Tolerance));
	}

	// Seven elements with duplicates
	{
		TArray<double> Values = {1.0, 2.0, 2.0, 3.0, 4.0, 5.0, 6.0};
		TestTrue(TEXT("Median with duplicates = 3"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 3.0, Tolerance));
	}

	return true;
}

/**
 * Test GetMedian with even count
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMedianEvenTest,
	"PCGEx.Unit.Math.Mean.GetMedian.EvenCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMedianEvenTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Two elements
	{
		TArray<double> Values = {1.0, 3.0};
		TestTrue(TEXT("Median(1,3) = 2 (average of middle two)"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2.0, Tolerance));
	}

	// Four elements
	{
		TArray<double> Values = {1.0, 2.0, 3.0, 4.0};
		TestTrue(TEXT("Median(1,2,3,4) = 2.5"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2.5, Tolerance));
	}

	// Four elements unsorted
	{
		TArray<double> Values = {4.0, 2.0, 1.0, 3.0};
		TestTrue(TEXT("Median(4,2,1,3) = 2.5"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2.5, Tolerance));
	}

	// Six elements
	{
		TArray<double> Values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
		TestTrue(TEXT("Median(1,2,3,4,5,6) = 3.5"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 3.5, Tolerance));
	}

	return true;
}

/**
 * Test GetMedian with edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMedianEdgeCasesTest,
	"PCGEx.Unit.Math.Mean.GetMedian.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMedianEdgeCasesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Empty array
	{
		TArray<double> Values;
		TestTrue(TEXT("Median of empty array = 0"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 0.0, Tolerance));
	}

	// All same values
	{
		TArray<double> Values = {5.0, 5.0, 5.0, 5.0, 5.0};
		TestTrue(TEXT("Median of identical values = that value"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 5.0, Tolerance));
	}

	// Negative values
	{
		TArray<double> Values = {-5.0, -3.0, -1.0};
		TestTrue(TEXT("Median(-5,-3,-1) = -3"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), -3.0, Tolerance));
	}

	// Mixed positive/negative
	{
		TArray<double> Values = {-2.0, 0.0, 2.0};
		TestTrue(TEXT("Median(-2,0,2) = 0"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 0.0, Tolerance));
	}

	// Large values
	{
		TArray<double> Values = {1000000.0, 2000000.0, 3000000.0};
		TestTrue(TEXT("Median of large values"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 2000000.0, Tolerance));
	}

	// Very small differences
	{
		TArray<double> Values = {1.0001, 1.0002, 1.0003};
		TestTrue(TEXT("Median of very close values"),
		         FMath::IsNearlyEqual(PCGExMath::GetMedian(Values), 1.0002, 0.0001));
	}

	return true;
}

/**
 * Test GetMedian with integer values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMedianIntTest,
	"PCGEx.Unit.Math.Mean.GetMedian.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMedianIntTest::RunTest(const FString& Parameters)
{
	// Odd count
	{
		TArray<int32> Values = {3, 1, 2};
		TestEqual(TEXT("Median(3,1,2) = 2"), PCGExMath::GetMedian(Values), 2);
	}

	// Even count (note: integer division)
	{
		TArray<int32> Values = {1, 2, 3, 4};
		// (2+3)/2 = 2 for integer division
		TestEqual(TEXT("Median(1,2,3,4) = 2 (int)"), PCGExMath::GetMedian(Values), 2);
	}

	// Larger set
	{
		TArray<int32> Values = {9, 1, 5, 3, 7, 2, 8, 4, 6};
		TestEqual(TEXT("Median(9,1,5,3,7,2,8,4,6) = 5"), PCGExMath::GetMedian(Values), 5);
	}

	return true;
}

/**
 * Test that GetMedian doesn't modify the original array
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMedianNonDestructiveTest,
	"PCGEx.Unit.Math.Mean.GetMedian.NonDestructive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMedianNonDestructiveTest::RunTest(const FString& Parameters)
{
	// Original array should remain unchanged after GetMedian
	TArray<double> Values = {5.0, 1.0, 3.0, 4.0, 2.0};
	TArray<double> Original = Values; // Copy

	PCGExMath::GetMedian(Values);

	TestEqual(TEXT("Array size unchanged"), Values.Num(), Original.Num());
	for (int32 i = 0; i < Values.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("Element %d unchanged"), i),
		         FMath::IsNearlyEqual(Values[i], Original[i], KINDA_SMALL_NUMBER));
	}

	return true;
}

// =============================================================================
// QuickSelect Algorithm Tests
// =============================================================================

/**
 * Test QuickSelect for finding k-th smallest element
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanQuickSelectTest,
	"PCGEx.Unit.Math.Mean.QuickSelect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanQuickSelectTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Find minimum (k=0)
	{
		TArray<double> Values = {5.0, 1.0, 3.0, 4.0, 2.0};
		double Result = PCGExMath::QuickSelect(Values, 0, Values.Num() - 1, 0);
		TestTrue(TEXT("QuickSelect k=0 finds minimum"),
		         FMath::IsNearlyEqual(Result, 1.0, Tolerance));
	}

	// Find maximum (k=n-1)
	{
		TArray<double> Values = {5.0, 1.0, 3.0, 4.0, 2.0};
		double Result = PCGExMath::QuickSelect(Values, 0, Values.Num() - 1, Values.Num() - 1);
		TestTrue(TEXT("QuickSelect k=n-1 finds maximum"),
		         FMath::IsNearlyEqual(Result, 5.0, Tolerance));
	}

	// Find median position
	{
		TArray<double> Values = {5.0, 1.0, 3.0, 4.0, 2.0};
		double Result = PCGExMath::QuickSelect(Values, 0, Values.Num() - 1, 2); // Middle index
		TestTrue(TEXT("QuickSelect k=2 finds 3rd smallest"),
		         FMath::IsNearlyEqual(Result, 3.0, Tolerance));
	}

	return true;
}

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExMeanMethod enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMethodEnumTest,
	"PCGEx.Unit.Math.Mean.Enums.MeanMethod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMethodEnumTest::RunTest(const FString& Parameters)
{
	// Test enum values are distinct
	TestTrue(TEXT("Average != Median"),
	         EPCGExMeanMethod::Average != EPCGExMeanMethod::Median);
	TestTrue(TEXT("Median != ModeMin"),
	         EPCGExMeanMethod::Median != EPCGExMeanMethod::ModeMin);
	TestTrue(TEXT("ModeMin != ModeMax"),
	         EPCGExMeanMethod::ModeMin != EPCGExMeanMethod::ModeMax);
	TestTrue(TEXT("ModeMax != Central"),
	         EPCGExMeanMethod::ModeMax != EPCGExMeanMethod::Central);
	TestTrue(TEXT("Central != Fixed"),
	         EPCGExMeanMethod::Central != EPCGExMeanMethod::Fixed);

	// Test underlying values
	TestEqual(TEXT("Average = 0"), static_cast<uint8>(EPCGExMeanMethod::Average), 0);
	TestEqual(TEXT("Median = 1"), static_cast<uint8>(EPCGExMeanMethod::Median), 1);
	TestEqual(TEXT("ModeMin = 2"), static_cast<uint8>(EPCGExMeanMethod::ModeMin), 2);
	TestEqual(TEXT("ModeMax = 3"), static_cast<uint8>(EPCGExMeanMethod::ModeMax), 3);
	TestEqual(TEXT("Central = 4"), static_cast<uint8>(EPCGExMeanMethod::Central), 4);
	TestEqual(TEXT("Fixed = 5"), static_cast<uint8>(EPCGExMeanMethod::Fixed), 5);

	return true;
}

/**
 * Test EPCGExMeanMeasure enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathMeanMeasureEnumTest,
	"PCGEx.Unit.Math.Mean.Enums.MeanMeasure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathMeanMeasureEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Relative = 0"), static_cast<uint8>(EPCGExMeanMeasure::Relative), 0);
	TestEqual(TEXT("Discrete = 1"), static_cast<uint8>(EPCGExMeanMeasure::Discrete), 1);

	return true;
}
