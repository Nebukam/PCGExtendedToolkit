// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExCompare Unit Tests
 *
 * Tests comparison functions in PCGExCompare.h:
 * - StrictlyEqual / StrictlyNotEqual
 * - EqualOrGreater / EqualOrSmaller
 * - StrictlyGreater / StrictlySmaller
 * - NearlyEqual / NearlyNotEqual
 * - Compare (main dispatcher)
 * - String comparisons
 *
 * Test naming convention: PCGEx.Unit.Utils.Compare.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Utils/PCGExCompare.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// StrictlyEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyEqualIntTest,
	"PCGEx.Unit.Utils.Compare.StrictlyEqual.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyEqualIntTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("5 == 5"), PCGExCompare::StrictlyEqual(5, 5));
	TestFalse(TEXT("5 != 6"), PCGExCompare::StrictlyEqual(5, 6));
	TestTrue(TEXT("0 == 0"), PCGExCompare::StrictlyEqual(0, 0));
	TestTrue(TEXT("-1 == -1"), PCGExCompare::StrictlyEqual(-1, -1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyEqualDoubleTest,
	"PCGEx.Unit.Utils.Compare.StrictlyEqual.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyEqualDoubleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("1.5 == 1.5"), PCGExCompare::StrictlyEqual(1.5, 1.5));
	TestFalse(TEXT("1.5 != 1.6"), PCGExCompare::StrictlyEqual(1.5, 1.6));

	// Note: floating point exact equality can be problematic
	double A = 0.1 + 0.2;  // May not be exactly 0.3
	TestTrue(TEXT("0.3 == 0.3 (exact)"), PCGExCompare::StrictlyEqual(0.3, 0.3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyEqualBoolTest,
	"PCGEx.Unit.Utils.Compare.StrictlyEqual.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyEqualBoolTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("true == true"), PCGExCompare::StrictlyEqual(true, true));
	TestTrue(TEXT("false == false"), PCGExCompare::StrictlyEqual(false, false));
	TestFalse(TEXT("true != false"), PCGExCompare::StrictlyEqual(true, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyEqualVectorTest,
	"PCGEx.Unit.Utils.Compare.StrictlyEqual.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyEqualVectorTest::RunTest(const FString& Parameters)
{
	FVector A(1, 2, 3);
	FVector B(1, 2, 3);
	FVector C(1, 2, 4);

	TestTrue(TEXT("Same vector"), PCGExCompare::StrictlyEqual(A, B));
	TestFalse(TEXT("Different vectors"), PCGExCompare::StrictlyEqual(A, C));

	return true;
}

// =============================================================================
// StrictlyNotEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyNotEqualTest,
	"PCGEx.Unit.Utils.Compare.StrictlyNotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyNotEqualTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("5 != 6"), PCGExCompare::StrictlyNotEqual(5, 6));
	TestFalse(TEXT("5 !not!= 5"), PCGExCompare::StrictlyNotEqual(5, 5));
	TestTrue(TEXT("1.0 != 2.0"), PCGExCompare::StrictlyNotEqual(1.0, 2.0));

	return true;
}

// =============================================================================
// EqualOrGreater Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEqualOrGreaterIntTest,
	"PCGEx.Unit.Utils.Compare.EqualOrGreater.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEqualOrGreaterIntTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("5 >= 5"), PCGExCompare::EqualOrGreater(5, 5));
	TestTrue(TEXT("6 >= 5"), PCGExCompare::EqualOrGreater(6, 5));
	TestFalse(TEXT("4 not >= 5"), PCGExCompare::EqualOrGreater(4, 5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEqualOrGreaterDoubleTest,
	"PCGEx.Unit.Utils.Compare.EqualOrGreater.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEqualOrGreaterDoubleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("1.5 >= 1.5"), PCGExCompare::EqualOrGreater(1.5, 1.5));
	TestTrue(TEXT("1.6 >= 1.5"), PCGExCompare::EqualOrGreater(1.6, 1.5));
	TestFalse(TEXT("1.4 not >= 1.5"), PCGExCompare::EqualOrGreater(1.4, 1.5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEqualOrGreaterBoolTest,
	"PCGEx.Unit.Utils.Compare.EqualOrGreater.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEqualOrGreaterBoolTest::RunTest(const FString& Parameters)
{
	// Bool comparison: true > false
	TestTrue(TEXT("true >= true"), PCGExCompare::EqualOrGreater(true, true));
	TestTrue(TEXT("true >= false"), PCGExCompare::EqualOrGreater(true, false));
	TestTrue(TEXT("false >= false"), PCGExCompare::EqualOrGreater(false, false));
	TestFalse(TEXT("false not >= true"), PCGExCompare::EqualOrGreater(false, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEqualOrGreaterVectorTest,
	"PCGEx.Unit.Utils.Compare.EqualOrGreater.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEqualOrGreaterVectorTest::RunTest(const FString& Parameters)
{
	// Vector comparison uses squared length
	FVector A(3, 4, 0);  // Length = 5
	FVector B(0, 3, 0);  // Length = 3
	FVector C(3, 4, 0);  // Length = 5

	TestTrue(TEXT("Longer vector >= shorter"), PCGExCompare::EqualOrGreater(A, B));
	TestTrue(TEXT("Equal length vectors"), PCGExCompare::EqualOrGreater(A, C));
	TestFalse(TEXT("Shorter not >= longer"), PCGExCompare::EqualOrGreater(B, A));

	return true;
}

// =============================================================================
// EqualOrSmaller Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEqualOrSmallerTest,
	"PCGEx.Unit.Utils.Compare.EqualOrSmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEqualOrSmallerTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("5 <= 5"), PCGExCompare::EqualOrSmaller(5, 5));
	TestTrue(TEXT("4 <= 5"), PCGExCompare::EqualOrSmaller(4, 5));
	TestFalse(TEXT("6 not <= 5"), PCGExCompare::EqualOrSmaller(6, 5));

	TestTrue(TEXT("1.4 <= 1.5"), PCGExCompare::EqualOrSmaller(1.4, 1.5));

	return true;
}

// =============================================================================
// StrictlyGreater Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyGreaterIntTest,
	"PCGEx.Unit.Utils.Compare.StrictlyGreater.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyGreaterIntTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("6 > 5"), PCGExCompare::StrictlyGreater(6, 5));
	TestFalse(TEXT("5 not > 5"), PCGExCompare::StrictlyGreater(5, 5));
	TestFalse(TEXT("4 not > 5"), PCGExCompare::StrictlyGreater(4, 5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlyGreaterBoolTest,
	"PCGEx.Unit.Utils.Compare.StrictlyGreater.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlyGreaterBoolTest::RunTest(const FString& Parameters)
{
	// true > false, but not true > true or false > false
	TestTrue(TEXT("true > false"), PCGExCompare::StrictlyGreater(true, false));
	TestFalse(TEXT("true not > true"), PCGExCompare::StrictlyGreater(true, true));
	TestFalse(TEXT("false not > true"), PCGExCompare::StrictlyGreater(false, true));
	TestFalse(TEXT("false not > false"), PCGExCompare::StrictlyGreater(false, false));

	return true;
}

// =============================================================================
// StrictlySmaller Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStrictlySmallerTest,
	"PCGEx.Unit.Utils.Compare.StrictlySmaller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStrictlySmallerTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("4 < 5"), PCGExCompare::StrictlySmaller(4, 5));
	TestFalse(TEXT("5 not < 5"), PCGExCompare::StrictlySmaller(5, 5));
	TestFalse(TEXT("6 not < 5"), PCGExCompare::StrictlySmaller(6, 5));

	// Bool
	TestTrue(TEXT("false < true"), PCGExCompare::StrictlySmaller(false, true));
	TestFalse(TEXT("true not < false"), PCGExCompare::StrictlySmaller(true, false));

	return true;
}

// =============================================================================
// NearlyEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareNearlyEqualDoubleTest,
	"PCGEx.Unit.Utils.Compare.NearlyEqual.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareNearlyEqualDoubleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	TestTrue(TEXT("1.0 ~= 1.0"), PCGExCompare::NearlyEqual(1.0, 1.0, Tolerance));
	TestTrue(TEXT("1.0 ~= 1.005"), PCGExCompare::NearlyEqual(1.0, 1.005, Tolerance));
	TestFalse(TEXT("1.0 !~= 1.02"), PCGExCompare::NearlyEqual(1.0, 1.02, Tolerance));

	// Floating point classic issue
	double A = 0.1 + 0.2;
	TestTrue(TEXT("0.1+0.2 ~= 0.3"), PCGExCompare::NearlyEqual(A, 0.3, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareNearlyEqualBoolTest,
	"PCGEx.Unit.Utils.Compare.NearlyEqual.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareNearlyEqualBoolTest::RunTest(const FString& Parameters)
{
	// Bool nearly equal is just equal
	TestTrue(TEXT("true ~= true"), PCGExCompare::NearlyEqual(true, true));
	TestTrue(TEXT("false ~= false"), PCGExCompare::NearlyEqual(false, false));
	TestFalse(TEXT("true !~= false"), PCGExCompare::NearlyEqual(true, false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareNearlyEqualVectorTest,
	"PCGEx.Unit.Utils.Compare.NearlyEqual.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareNearlyEqualVectorTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	FVector A(1.0, 2.0, 3.0);
	FVector B(1.005, 2.005, 3.005);
	FVector C(1.1, 2.1, 3.1);

	TestTrue(TEXT("Nearly equal vectors"), PCGExCompare::NearlyEqual(A, B, Tolerance));
	TestFalse(TEXT("Not nearly equal vectors"), PCGExCompare::NearlyEqual(A, C, Tolerance));

	return true;
}

// =============================================================================
// NearlyNotEqual Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareNearlyNotEqualTest,
	"PCGEx.Unit.Utils.Compare.NearlyNotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareNearlyNotEqualTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	TestFalse(TEXT("1.0 not !~= 1.0"), PCGExCompare::NearlyNotEqual(1.0, 1.0, Tolerance));
	TestTrue(TEXT("1.0 !~= 1.1"), PCGExCompare::NearlyNotEqual(1.0, 1.1, Tolerance));

	return true;
}

// =============================================================================
// Compare (Main Dispatcher) Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareDispatcherIntTest,
	"PCGEx.Unit.Utils.Compare.Dispatcher.Int",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareDispatcherIntTest::RunTest(const FString& Parameters)
{
	int32 A = 5;
	int32 B = 5;
	int32 C = 3;
	int32 D = 7;

	TestTrue(TEXT("Compare == "), PCGExCompare::Compare(EPCGExComparison::StrictlyEqual, A, B));
	TestTrue(TEXT("Compare != "), PCGExCompare::Compare(EPCGExComparison::StrictlyNotEqual, A, C));
	TestTrue(TEXT("Compare >= (equal)"), PCGExCompare::Compare(EPCGExComparison::EqualOrGreater, A, B));
	TestTrue(TEXT("Compare >= (greater)"), PCGExCompare::Compare(EPCGExComparison::EqualOrGreater, A, C));
	TestTrue(TEXT("Compare <= (equal)"), PCGExCompare::Compare(EPCGExComparison::EqualOrSmaller, A, B));
	TestTrue(TEXT("Compare <= (smaller)"), PCGExCompare::Compare(EPCGExComparison::EqualOrSmaller, A, D));
	TestTrue(TEXT("Compare > "), PCGExCompare::Compare(EPCGExComparison::StrictlyGreater, A, C));
	TestTrue(TEXT("Compare < "), PCGExCompare::Compare(EPCGExComparison::StrictlySmaller, A, D));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareDispatcherDoubleTest,
	"PCGEx.Unit.Utils.Compare.Dispatcher.Double",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareDispatcherDoubleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	double A = 1.0;
	double B = 1.005;
	double C = 2.0;

	TestTrue(TEXT("Compare ~= (with tolerance)"),
		PCGExCompare::Compare(EPCGExComparison::NearlyEqual, A, B, Tolerance));
	TestTrue(TEXT("Compare !~= (with tolerance)"),
		PCGExCompare::Compare(EPCGExComparison::NearlyNotEqual, A, C, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareDispatcherVectorTest,
	"PCGEx.Unit.Utils.Compare.Dispatcher.Vector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareDispatcherVectorTest::RunTest(const FString& Parameters)
{
	FVector A(3, 4, 0);  // Length 5
	FVector B(0, 3, 0);  // Length 3
	FVector C(3, 4, 0);  // Length 5

	TestTrue(TEXT("Vector =="), PCGExCompare::Compare(EPCGExComparison::StrictlyEqual, A, C));
	TestTrue(TEXT("Vector >="), PCGExCompare::Compare(EPCGExComparison::EqualOrGreater, A, B));
	TestTrue(TEXT("Vector >"), PCGExCompare::Compare(EPCGExComparison::StrictlyGreater, A, B));

	return true;
}

// =============================================================================
// String Comparison Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringEqualTest,
	"PCGEx.Unit.Utils.Compare.String.Equal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringEqualTest::RunTest(const FString& Parameters)
{
	FString A = TEXT("Hello");
	FString B = TEXT("Hello");
	FString C = TEXT("World");

	TestTrue(TEXT("String =="),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyEqual, A, B));
	TestFalse(TEXT("String != same"),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyEqual, A, C));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringNotEqualTest,
	"PCGEx.Unit.Utils.Compare.String.NotEqual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringNotEqualTest::RunTest(const FString& Parameters)
{
	FString A = TEXT("Hello");
	FString B = TEXT("World");

	TestTrue(TEXT("String !="),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyNotEqual, A, B));
	TestFalse(TEXT("String not != same"),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyNotEqual, A, A));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringLengthTest,
	"PCGEx.Unit.Utils.Compare.String.Length",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringLengthTest::RunTest(const FString& Parameters)
{
	FString Short = TEXT("Hi");
	FString Medium = TEXT("Hello");
	FString Long = TEXT("Hello World");

	TestTrue(TEXT("Length equal"),
		PCGExCompare::Compare(EPCGExStringComparison::LengthStrictlyEqual, Medium, TEXT("World")));
	TestTrue(TEXT("Length not equal"),
		PCGExCompare::Compare(EPCGExStringComparison::LengthStrictlyUnequal, Short, Long));
	TestTrue(TEXT("Length >="),
		PCGExCompare::Compare(EPCGExStringComparison::LengthEqualOrGreater, Long, Short));
	TestTrue(TEXT("Length <="),
		PCGExCompare::Compare(EPCGExStringComparison::LengthEqualOrSmaller, Short, Long));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringContainsTest,
	"PCGEx.Unit.Utils.Compare.String.Contains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringContainsTest::RunTest(const FString& Parameters)
{
	FString Full = TEXT("Hello World");
	FString Sub = TEXT("World");
	FString NotSub = TEXT("Foo");

	TestTrue(TEXT("String contains"),
		PCGExCompare::Compare(EPCGExStringComparison::Contains, Full, Sub));
	TestFalse(TEXT("String not contains"),
		PCGExCompare::Compare(EPCGExStringComparison::Contains, Full, NotSub));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringStartsWithTest,
	"PCGEx.Unit.Utils.Compare.String.StartsWith",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringStartsWithTest::RunTest(const FString& Parameters)
{
	FString Full = TEXT("Hello World");

	TestTrue(TEXT("Starts with Hello"),
		PCGExCompare::Compare(EPCGExStringComparison::StartsWith, Full, TEXT("Hello")));
	TestFalse(TEXT("Not starts with World"),
		PCGExCompare::Compare(EPCGExStringComparison::StartsWith, Full, TEXT("World")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareStringEndsWithTest,
	"PCGEx.Unit.Utils.Compare.String.EndsWith",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareStringEndsWithTest::RunTest(const FString& Parameters)
{
	FString Full = TEXT("Hello World");

	TestTrue(TEXT("Ends with World"),
		PCGExCompare::Compare(EPCGExStringComparison::EndsWith, Full, TEXT("World")));
	TestFalse(TEXT("Not ends with Hello"),
		PCGExCompare::Compare(EPCGExStringComparison::EndsWith, Full, TEXT("Hello")));

	return true;
}

// =============================================================================
// ToString Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareToStringTest,
	"PCGEx.Unit.Utils.Compare.ToString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareToStringTest::RunTest(const FString& Parameters)
{
	// Verify ToString returns non-empty strings
	FString EqStr = PCGExCompare::ToString(EPCGExComparison::StrictlyEqual);
	TestTrue(TEXT("ToString StrictlyEqual non-empty"), !EqStr.IsEmpty());

	FString NeStr = PCGExCompare::ToString(EPCGExComparison::NearlyEqual);
	TestTrue(TEXT("ToString NearlyEqual non-empty"), !NeStr.IsEmpty());

	FString StrEq = PCGExCompare::ToString(EPCGExStringComparison::StrictlyEqual);
	TestTrue(TEXT("ToString String StrictlyEqual non-empty"), !StrEq.IsEmpty());

	FString ContainsStr = PCGExCompare::ToString(EPCGExStringComparison::Contains);
	TestTrue(TEXT("ToString String Contains non-empty"), !ContainsStr.IsEmpty());

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEdgeCasesTest,
	"PCGEx.Unit.Utils.Compare.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Zero values
	TestTrue(TEXT("0 == 0"), PCGExCompare::StrictlyEqual(0, 0));
	TestTrue(TEXT("0.0 == 0.0"), PCGExCompare::StrictlyEqual(0.0, 0.0));

	// Negative values
	TestTrue(TEXT("-5 < 5"), PCGExCompare::StrictlySmaller(-5, 5));
	TestTrue(TEXT("-5 < 0"), PCGExCompare::StrictlySmaller(-5, 0));

	// Large values
	TestTrue(TEXT("Large value comparison"),
		PCGExCompare::StrictlyGreater(1000000.0, 999999.0));

	// Small tolerance differences
	const double SmallTolerance = 1e-10;
	TestTrue(TEXT("Very small tolerance equal"),
		PCGExCompare::NearlyEqual(1.0, 1.0 + 1e-12, SmallTolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareEmptyStringsTest,
	"PCGEx.Unit.Utils.Compare.String.Empty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareEmptyStringsTest::RunTest(const FString& Parameters)
{
	FString Empty = TEXT("");
	FString NonEmpty = TEXT("Hello");

	TestTrue(TEXT("Empty == Empty"),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyEqual, Empty, Empty));
	TestTrue(TEXT("Empty != NonEmpty"),
		PCGExCompare::Compare(EPCGExStringComparison::StrictlyNotEqual, Empty, NonEmpty));
	TestTrue(TEXT("Empty length < NonEmpty length"),
		PCGExCompare::Compare(EPCGExStringComparison::LengthEqualOrSmaller, Empty, NonEmpty));

	return true;
}

// =============================================================================
// FVector2D Comparison Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareVector2DTest,
	"PCGEx.Unit.Utils.Compare.Vector2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareVector2DTest::RunTest(const FString& Parameters)
{
	FVector2D A(3, 4);  // Length 5
	FVector2D B(0, 3);  // Length 3
	FVector2D C(3, 4);  // Same as A

	TestTrue(TEXT("Vector2D =="), PCGExCompare::StrictlyEqual(A, C));
	TestFalse(TEXT("Vector2D !="), PCGExCompare::StrictlyEqual(A, B));
	TestTrue(TEXT("Vector2D >="), PCGExCompare::EqualOrGreater(A, B));
	TestTrue(TEXT("Vector2D >"), PCGExCompare::StrictlyGreater(A, B));
	TestFalse(TEXT("Vector2D not >"), PCGExCompare::StrictlyGreater(B, A));

	return true;
}

// =============================================================================
// FTransform Comparison Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareTransformTest,
	"PCGEx.Unit.Utils.Compare.Transform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareTransformTest::RunTest(const FString& Parameters)
{
	FTransform A = FTransform::Identity;
	FTransform B = FTransform::Identity;
	FTransform C(FQuat::Identity, FVector(100, 0, 0), FVector::OneVector);

	TestTrue(TEXT("Transform =="), PCGExCompare::StrictlyEqual(A, B));
	TestFalse(TEXT("Transform !="), PCGExCompare::StrictlyEqual(A, C));

	// Transform comparison uses component-wise comparison
	TestTrue(TEXT("Transform >="),
		PCGExCompare::EqualOrGreater(C, A));  // C has larger location

	return true;
}

// =============================================================================
// FRotator Comparison Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompareRotatorTest,
	"PCGEx.Unit.Utils.Compare.Rotator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompareRotatorTest::RunTest(const FString& Parameters)
{
	FRotator A(0, 0, 0);
	FRotator B(0, 0, 0);
	FRotator C(45, 90, 0);

	TestTrue(TEXT("Rotator =="), PCGExCompare::StrictlyEqual(A, B));
	TestFalse(TEXT("Rotator !="), PCGExCompare::StrictlyEqual(A, C));

	return true;
}
