// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExTypeOpsNumeric Unit Tests
 *
 * Tests type operations for numeric types in PCGExTypeOpsNumeric.h:
 * - FTypeOps<bool>: Boolean operations and conversions
 * - FTypeOps<int32>: Integer operations and conversions
 * - FTypeOps<float>: Float operations and conversions
 * - FTypeOps<double>: Double operations and conversions
 *
 * Test categories:
 * - Conversions between types
 * - Blend operations (Add, Sub, Mult, Min, Max, Lerp, etc.)
 * - Hash operations
 *
 * Test naming convention: PCGEx.Unit.Types.TypeOps.<Type>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypeOpsNumeric.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Bool Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsBoolDefaultTest,
	"PCGEx.Unit.Types.TypeOps.Bool.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsBoolDefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Bool default is false"),
		PCGExTypeOps::FTypeOps<bool>::GetDefault(), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsBoolConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Bool.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsBoolConversionsTest::RunTest(const FString& Parameters)
{
	using BoolOps = PCGExTypeOps::FTypeOps<bool>;

	// Bool to numeric
	TestEqual(TEXT("true -> int32 = 1"), BoolOps::ConvertTo<int32>(true), 1);
	TestEqual(TEXT("false -> int32 = 0"), BoolOps::ConvertTo<int32>(false), 0);
	TestTrue(TEXT("true -> double = 1.0"),
		FMath::IsNearlyEqual(BoolOps::ConvertTo<double>(true), 1.0));
	TestTrue(TEXT("false -> double = 0.0"),
		FMath::IsNearlyEqual(BoolOps::ConvertTo<double>(false), 0.0));

	// Bool to string
	TestEqual(TEXT("true -> FString"),
		BoolOps::ConvertTo<FString>(true), FString(TEXT("true")));
	TestEqual(TEXT("false -> FString"),
		BoolOps::ConvertTo<FString>(false), FString(TEXT("false")));

	// Numeric to bool
	TestTrue(TEXT("1 -> bool = true"), BoolOps::ConvertFrom<int32>(1));
	TestFalse(TEXT("0 -> bool = false"), BoolOps::ConvertFrom<int32>(0));
	TestFalse(TEXT("-1 -> bool = false"), BoolOps::ConvertFrom<int32>(-1));
	TestTrue(TEXT("0.5 -> bool = true"), BoolOps::ConvertFrom<double>(0.5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsBoolBlendTest,
	"PCGEx.Unit.Types.TypeOps.Bool.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsBoolBlendTest::RunTest(const FString& Parameters)
{
	using BoolOps = PCGExTypeOps::FTypeOps<bool>;

	// Add = OR
	TestTrue(TEXT("Add(true, true) = true"), BoolOps::Add(true, true));
	TestTrue(TEXT("Add(true, false) = true"), BoolOps::Add(true, false));
	TestTrue(TEXT("Add(false, true) = true"), BoolOps::Add(false, true));
	TestFalse(TEXT("Add(false, false) = false"), BoolOps::Add(false, false));

	// Mult = AND
	TestTrue(TEXT("Mult(true, true) = true"), BoolOps::Mult(true, true));
	TestFalse(TEXT("Mult(true, false) = false"), BoolOps::Mult(true, false));
	TestFalse(TEXT("Mult(false, true) = false"), BoolOps::Mult(false, true));
	TestFalse(TEXT("Mult(false, false) = false"), BoolOps::Mult(false, false));

	// Sub = A AND NOT B
	TestFalse(TEXT("Sub(true, true) = false"), BoolOps::Sub(true, true));
	TestTrue(TEXT("Sub(true, false) = true"), BoolOps::Sub(true, false));
	TestFalse(TEXT("Sub(false, true) = false"), BoolOps::Sub(false, true));
	TestFalse(TEXT("Sub(false, false) = false"), BoolOps::Sub(false, false));

	// Min = AND, Max = OR
	TestTrue(TEXT("Min(true, true) = true"), BoolOps::Min(true, true));
	TestFalse(TEXT("Min(true, false) = false"), BoolOps::Min(true, false));
	TestTrue(TEXT("Max(true, false) = true"), BoolOps::Max(true, false));
	TestFalse(TEXT("Max(false, false) = false"), BoolOps::Max(false, false));

	// Lerp
	TestTrue(TEXT("Lerp(true, false, 0.3) = true (W < 0.5)"), BoolOps::Lerp(true, false, 0.3));
	TestFalse(TEXT("Lerp(true, false, 0.7) = false (W >= 0.5)"), BoolOps::Lerp(true, false, 0.7));

	return true;
}

// =============================================================================
// Int32 Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsInt32DefaultTest,
	"PCGEx.Unit.Types.TypeOps.Int32.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsInt32DefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Int32 default is 0"),
		PCGExTypeOps::FTypeOps<int32>::GetDefault(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsInt32ConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Int32.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsInt32ConversionsTest::RunTest(const FString& Parameters)
{
	using Int32Ops = PCGExTypeOps::FTypeOps<int32>;

	// Int32 to other types
	TestTrue(TEXT("5 -> bool = true"), Int32Ops::ConvertTo<bool>(5));
	TestFalse(TEXT("0 -> bool = false"), Int32Ops::ConvertTo<bool>(0));
	TestTrue(TEXT("42 -> double"),
		FMath::IsNearlyEqual(Int32Ops::ConvertTo<double>(42), 42.0));
	TestEqual(TEXT("123 -> FString"),
		Int32Ops::ConvertTo<FString>(123), FString(TEXT("123")));

	// Other types to int32
	TestEqual(TEXT("true -> int32 = 1"), Int32Ops::ConvertFrom<bool>(true), 1);
	TestEqual(TEXT("false -> int32 = 0"), Int32Ops::ConvertFrom<bool>(false), 0);
	TestEqual(TEXT("3.7 -> int32 = 3"), Int32Ops::ConvertFrom<double>(3.7), 3);
	TestEqual(TEXT("-2.9 -> int32 = -2"), Int32Ops::ConvertFrom<double>(-2.9), -2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsInt32BlendTest,
	"PCGEx.Unit.Types.TypeOps.Int32.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsInt32BlendTest::RunTest(const FString& Parameters)
{
	using Int32Ops = PCGExTypeOps::FTypeOps<int32>;

	// Basic arithmetic
	TestEqual(TEXT("Add(3, 5) = 8"), Int32Ops::Add(3, 5), 8);
	TestEqual(TEXT("Sub(10, 4) = 6"), Int32Ops::Sub(10, 4), 6);
	TestEqual(TEXT("Mult(6, 7) = 42"), Int32Ops::Mult(6, 7), 42);
	TestEqual(TEXT("Div(15, 3.0) = 5"), Int32Ops::Div(15, 3.0), 5);

	// Min/Max
	TestEqual(TEXT("Min(3, 7) = 3"), Int32Ops::Min(3, 7), 3);
	TestEqual(TEXT("Max(3, 7) = 7"), Int32Ops::Max(3, 7), 7);
	TestEqual(TEXT("Min(-5, 2) = -5"), Int32Ops::Min(-5, 2), -5);

	// Average
	TestEqual(TEXT("Average(4, 6) = 5"), Int32Ops::Average(4, 6), 5);
	TestEqual(TEXT("Average(3, 4) = 3 (int truncation)"), Int32Ops::Average(3, 4), 3);

	// Lerp
	TestEqual(TEXT("Lerp(0, 10, 0.5) = 5"), Int32Ops::Lerp(0, 10, 0.5), 5);
	TestEqual(TEXT("Lerp(0, 10, 0.0) = 0"), Int32Ops::Lerp(0, 10, 0.0), 0);
	TestEqual(TEXT("Lerp(0, 10, 1.0) = 10"), Int32Ops::Lerp(0, 10, 1.0), 10);

	// Modulo
	TestEqual(TEXT("ModComplex(10, 3) = 1"), Int32Ops::ModComplex(10, 3), 1);
	TestEqual(TEXT("ModComplex(10, 0) = 10 (div by zero)"), Int32Ops::ModComplex(10, 0), 10);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsInt32UnsignedOpsTest,
	"PCGEx.Unit.Types.TypeOps.Int32.UnsignedOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsInt32UnsignedOpsTest::RunTest(const FString& Parameters)
{
	using Int32Ops = PCGExTypeOps::FTypeOps<int32>;

	// UnsignedMin - returns value with smaller absolute value
	TestEqual(TEXT("UnsignedMin(3, -5) = 3"), Int32Ops::UnsignedMin(3, -5), 3);
	TestEqual(TEXT("UnsignedMin(-2, 4) = -2"), Int32Ops::UnsignedMin(-2, 4), -2);
	TestEqual(TEXT("UnsignedMin(-3, 3) = -3 or 3"),
		FMath::Abs(Int32Ops::UnsignedMin(-3, 3)), 3);

	// UnsignedMax - returns value with larger absolute value
	TestEqual(TEXT("UnsignedMax(3, -5) = -5"), Int32Ops::UnsignedMax(3, -5), -5);
	TestEqual(TEXT("UnsignedMax(-2, 4) = 4"), Int32Ops::UnsignedMax(-2, 4), 4);

	// AbsoluteMin/Max
	TestEqual(TEXT("AbsoluteMin(-3, 5) = 3"), Int32Ops::AbsoluteMin(-3, 5), 3);
	TestEqual(TEXT("AbsoluteMax(-3, 5) = 5"), Int32Ops::AbsoluteMax(-3, 5), 5);

	// Abs
	TestEqual(TEXT("Abs(-7) = 7"), Int32Ops::Abs(-7), 7);
	TestEqual(TEXT("Abs(7) = 7"), Int32Ops::Abs(7), 7);

	return true;
}

// =============================================================================
// Float Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsFloatDefaultTest,
	"PCGEx.Unit.Types.TypeOps.Float.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsFloatDefaultTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Float default is 0.0f"),
		FMath::IsNearlyZero(PCGExTypeOps::FTypeOps<float>::GetDefault()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsFloatBlendTest,
	"PCGEx.Unit.Types.TypeOps.Float.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsFloatBlendTest::RunTest(const FString& Parameters)
{
	using FloatOps = PCGExTypeOps::FTypeOps<float>;
	const float Tolerance = 0.001f;

	// Basic arithmetic
	TestTrue(TEXT("Add(1.5, 2.5) = 4.0"),
		FMath::IsNearlyEqual(FloatOps::Add(1.5f, 2.5f), 4.0f, Tolerance));
	TestTrue(TEXT("Sub(5.0, 2.5) = 2.5"),
		FMath::IsNearlyEqual(FloatOps::Sub(5.0f, 2.5f), 2.5f, Tolerance));
	TestTrue(TEXT("Mult(2.5, 4.0) = 10.0"),
		FMath::IsNearlyEqual(FloatOps::Mult(2.5f, 4.0f), 10.0f, Tolerance));
	TestTrue(TEXT("Div(10.0, 4.0) = 2.5"),
		FMath::IsNearlyEqual(FloatOps::Div(10.0f, 4.0), 2.5f, Tolerance));

	// Lerp
	TestTrue(TEXT("Lerp(0, 10, 0.5) = 5"),
		FMath::IsNearlyEqual(FloatOps::Lerp(0.0f, 10.0f, 0.5), 5.0f, Tolerance));
	TestTrue(TEXT("Lerp(0, 10, 0.25) = 2.5"),
		FMath::IsNearlyEqual(FloatOps::Lerp(0.0f, 10.0f, 0.25), 2.5f, Tolerance));

	// Average
	TestTrue(TEXT("Average(3, 7) = 5"),
		FMath::IsNearlyEqual(FloatOps::Average(3.0f, 7.0f), 5.0f, Tolerance));

	// Modulo
	TestTrue(TEXT("ModComplex(5.5, 2.0) = 1.5"),
		FMath::IsNearlyEqual(FloatOps::ModComplex(5.5f, 2.0f), 1.5f, Tolerance));

	return true;
}

// =============================================================================
// Double Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsDoubleDefaultTest,
	"PCGEx.Unit.Types.TypeOps.Double.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsDoubleDefaultTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Double default is 0.0"),
		FMath::IsNearlyZero(PCGExTypeOps::FTypeOps<double>::GetDefault()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsDoubleConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Double.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsDoubleConversionsTest::RunTest(const FString& Parameters)
{
	using DoubleOps = PCGExTypeOps::FTypeOps<double>;

	// To other types
	TestTrue(TEXT("1.5 -> bool = true"), DoubleOps::ConvertTo<bool>(1.5));
	TestFalse(TEXT("0.0 -> bool = false"), DoubleOps::ConvertTo<bool>(0.0));
	TestFalse(TEXT("-1.0 -> bool = false"), DoubleOps::ConvertTo<bool>(-1.0));
	TestEqual(TEXT("3.7 -> int32 = 3"), DoubleOps::ConvertTo<int32>(3.7), 3);

	// From other types
	TestTrue(TEXT("int 42 -> double"),
		FMath::IsNearlyEqual(DoubleOps::ConvertFrom<int32>(42), 42.0));

	// Vector extracts X component
	FVector V(1.5, 2.5, 3.5);
	TestTrue(TEXT("FVector -> double (X component)"),
		FMath::IsNearlyEqual(DoubleOps::ConvertFrom<FVector>(V), 1.5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsDoubleBlendTest,
	"PCGEx.Unit.Types.TypeOps.Double.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsDoubleBlendTest::RunTest(const FString& Parameters)
{
	using DoubleOps = PCGExTypeOps::FTypeOps<double>;
	const double Tolerance = 0.0001;

	// Weighted operations
	TestTrue(TEXT("WeightedAdd(10, 5, 0.5) = 12.5"),
		FMath::IsNearlyEqual(DoubleOps::WeightedAdd(10.0, 5.0, 0.5), 12.5, Tolerance));
	TestTrue(TEXT("WeightedSub(10, 5, 0.5) = 7.5"),
		FMath::IsNearlyEqual(DoubleOps::WeightedSub(10.0, 5.0, 0.5), 7.5, Tolerance));

	// Weight normalization
	TestTrue(TEXT("NormalizeWeight(10, 2) = 5"),
		FMath::IsNearlyEqual(DoubleOps::NormalizeWeight(10.0, 2.0), 5.0, Tolerance));
	TestTrue(TEXT("NormalizeWeight(10, 0) = 10 (div by zero protection)"),
		FMath::IsNearlyEqual(DoubleOps::NormalizeWeight(10.0, 0.0), 10.0, Tolerance));

	// Factor
	TestTrue(TEXT("Factor(5, 3) = 15"),
		FMath::IsNearlyEqual(DoubleOps::Factor(5.0, 3.0), 15.0, Tolerance));

	// CopyA/CopyB
	TestTrue(TEXT("CopyA(5, 10) = 5"),
		FMath::IsNearlyEqual(DoubleOps::CopyA(5.0, 10.0), 5.0, Tolerance));
	TestTrue(TEXT("CopyB(5, 10) = 10"),
		FMath::IsNearlyEqual(DoubleOps::CopyB(5.0, 10.0), 10.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsDoubleUnsignedTest,
	"PCGEx.Unit.Types.TypeOps.Double.UnsignedOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsDoubleUnsignedTest::RunTest(const FString& Parameters)
{
	using DoubleOps = PCGExTypeOps::FTypeOps<double>;
	const double Tolerance = 0.0001;

	// UnsignedMin/Max
	TestTrue(TEXT("UnsignedMin(3.0, -5.0) = 3.0"),
		FMath::IsNearlyEqual(DoubleOps::UnsignedMin(3.0, -5.0), 3.0, Tolerance));
	TestTrue(TEXT("UnsignedMax(3.0, -5.0) = -5.0"),
		FMath::IsNearlyEqual(DoubleOps::UnsignedMax(3.0, -5.0), -5.0, Tolerance));

	// AbsoluteMin/Max
	TestTrue(TEXT("AbsoluteMin(-3.0, 5.0) = 3.0"),
		FMath::IsNearlyEqual(DoubleOps::AbsoluteMin(-3.0, 5.0), 3.0, Tolerance));
	TestTrue(TEXT("AbsoluteMax(-3.0, 5.0) = 5.0"),
		FMath::IsNearlyEqual(DoubleOps::AbsoluteMax(-3.0, 5.0), 5.0, Tolerance));

	// Abs
	TestTrue(TEXT("Abs(-7.5) = 7.5"),
		FMath::IsNearlyEqual(DoubleOps::Abs(-7.5), 7.5, Tolerance));

	return true;
}

// =============================================================================
// Cross-Type Conversion Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsCrossTypeTest,
	"PCGEx.Unit.Types.TypeOps.CrossType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsCrossTypeTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.0001;

	// Int -> Double -> Int round trip
	int32 OrigInt = 42;
	double AsDouble = PCGExTypeOps::FTypeOps<int32>::ConvertTo<double>(OrigInt);
	int32 BackToInt = PCGExTypeOps::FTypeOps<double>::ConvertTo<int32>(AsDouble);
	TestEqual(TEXT("Int->Double->Int round trip"), BackToInt, OrigInt);

	// Float -> Double -> Float
	float OrigFloat = 3.14159f;
	double AsDouble2 = PCGExTypeOps::FTypeOps<float>::ConvertTo<double>(OrigFloat);
	float BackToFloat = PCGExTypeOps::FTypeOps<double>::ConvertTo<float>(AsDouble2);
	TestTrue(TEXT("Float->Double->Float round trip"),
		FMath::IsNearlyEqual(BackToFloat, OrigFloat, 0.0001f));

	// Bool -> Int -> Bool
	bool OrigBool = true;
	int32 AsInt = PCGExTypeOps::FTypeOps<bool>::ConvertTo<int32>(OrigBool);
	bool BackToBool = PCGExTypeOps::FTypeOps<int32>::ConvertTo<bool>(AsInt);
	TestEqual(TEXT("Bool->Int->Bool round trip (true)"), BackToBool, OrigBool);

	OrigBool = false;
	AsInt = PCGExTypeOps::FTypeOps<bool>::ConvertTo<int32>(OrigBool);
	BackToBool = PCGExTypeOps::FTypeOps<int32>::ConvertTo<bool>(AsInt);
	TestEqual(TEXT("Bool->Int->Bool round trip (false)"), BackToBool, OrigBool);

	return true;
}

// =============================================================================
// Hash Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsHashTest,
	"PCGEx.Unit.Types.TypeOps.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsHashTest::RunTest(const FString& Parameters)
{
	using Int32Ops = PCGExTypeOps::FTypeOps<int32>;
	using DoubleOps = PCGExTypeOps::FTypeOps<double>;

	// Same values should produce same hash
	TestEqual(TEXT("Hash(5) == Hash(5)"),
		Int32Ops::Hash(5), Int32Ops::Hash(5));

	// Different values should (usually) produce different hashes
	TestNotEqual(TEXT("Hash(5) != Hash(6)"),
		Int32Ops::Hash(5), Int32Ops::Hash(6));

	// NaiveHash combines two values
	auto Hash1 = Int32Ops::NaiveHash(10, 20);
	auto Hash2 = Int32Ops::NaiveHash(10, 20);
	TestEqual(TEXT("NaiveHash is deterministic"), Hash1, Hash2);

	// UnsignedHash is order-independent
	auto UHash1 = Int32Ops::UnsignedHash(10, 20);
	auto UHash2 = Int32Ops::UnsignedHash(20, 10);
	TestEqual(TEXT("UnsignedHash(10,20) == UnsignedHash(20,10)"), UHash1, UHash2);

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsEdgeCasesTest,
	"PCGEx.Unit.Types.TypeOps.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsEdgeCasesTest::RunTest(const FString& Parameters)
{
	using DoubleOps = PCGExTypeOps::FTypeOps<double>;

	// Division by zero protection
	TestTrue(TEXT("Div by zero returns original"),
		FMath::IsNearlyEqual(DoubleOps::Div(10.0, 0.0), 10.0));

	// Modulo by zero protection
	TestTrue(TEXT("Mod by zero returns original"),
		FMath::IsNearlyEqual(DoubleOps::ModComplex(10.0, 0.0), 10.0));

	// Weight by zero protection
	TestTrue(TEXT("Weight by zero returns A"),
		FMath::IsNearlyEqual(DoubleOps::Weight(5.0, 10.0, 0.0), 5.0));

	// NormalizeWeight by zero protection
	TestTrue(TEXT("NormalizeWeight by zero returns original"),
		FMath::IsNearlyEqual(DoubleOps::NormalizeWeight(10.0, 0.0), 10.0));

	// Factor with zero
	TestTrue(TEXT("Factor with zero factor"),
		FMath::IsNearlyZero(DoubleOps::Factor(10.0, 0.0)));

	return true;
}
