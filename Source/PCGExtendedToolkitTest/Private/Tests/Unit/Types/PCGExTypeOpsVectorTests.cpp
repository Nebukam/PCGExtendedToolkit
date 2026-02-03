// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExTypeOpsVector Unit Tests
 *
 * Tests type operations for vector types in PCGExTypeOpsVector.h:
 * - FTypeOps<FVector2D>: 2D vector operations
 * - FTypeOps<FVector>: 3D vector operations
 * - FTypeOps<FVector4>: 4D vector operations
 *
 * Test categories:
 * - Default values
 * - Conversions between types
 * - Blend operations (Add, Sub, Mult, Min, Max, Lerp, etc.)
 * - Component-wise operations
 *
 * Test naming convention: PCGEx.Unit.Types.TypeOps.<Type>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Types/PCGExTypeOpsVector.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// FVector2D Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector2DDefaultTest,
	"PCGEx.Unit.Types.TypeOps.Vector2D.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector2DDefaultTest::RunTest(const FString& Parameters)
{
	FVector2D Default = PCGExTypeOps::FTypeOps<FVector2D>::GetDefault();
	TestTrue(TEXT("Vector2D default is zero"),
		FMath::IsNearlyZero(Default.X) && FMath::IsNearlyZero(Default.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector2DConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Vector2D.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector2DConversionsTest::RunTest(const FString& Parameters)
{
	using V2DOps = PCGExTypeOps::FTypeOps<FVector2D>;
	const double Tolerance = 0.001;

	FVector2D V(3, 4);

	// To scalar types (extracts X)
	TestTrue(TEXT("Vector2D -> double extracts X"),
		FMath::IsNearlyEqual(V2DOps::ConvertTo<double>(V), 3.0, Tolerance));

	// To bool (checks squared length > 0)
	TestTrue(TEXT("Non-zero Vector2D -> true"), V2DOps::ConvertTo<bool>(V));
	TestFalse(TEXT("Zero Vector2D -> false"),
		V2DOps::ConvertTo<bool>(FVector2D::ZeroVector));

	// To FVector (Z = 0)
	FVector AsV3 = V2DOps::ConvertTo<FVector>(V);
	TestTrue(TEXT("Vector2D -> FVector"),
		FMath::IsNearlyEqual(AsV3.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV3.Y, 4.0, Tolerance) &&
		FMath::IsNearlyZero(AsV3.Z, Tolerance));

	// From FVector
	FVector V3(1, 2, 3);
	FVector2D FromV3 = V2DOps::ConvertFrom<FVector>(V3);
	TestTrue(TEXT("FVector -> Vector2D"),
		FMath::IsNearlyEqual(FromV3.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(FromV3.Y, 2.0, Tolerance));

	// From scalar
	FVector2D FromDouble = V2DOps::ConvertFrom<double>(5.0);
	TestTrue(TEXT("double -> Vector2D (both components)"),
		FMath::IsNearlyEqual(FromDouble.X, 5.0, Tolerance) &&
		FMath::IsNearlyEqual(FromDouble.Y, 5.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector2DBlendTest,
	"PCGEx.Unit.Types.TypeOps.Vector2D.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector2DBlendTest::RunTest(const FString& Parameters)
{
	using V2DOps = PCGExTypeOps::FTypeOps<FVector2D>;
	const double Tolerance = 0.001;

	FVector2D A(2, 3);
	FVector2D B(4, 6);

	// Basic arithmetic
	FVector2D Sum = V2DOps::Add(A, B);
	TestTrue(TEXT("Add"), FMath::IsNearlyEqual(Sum.X, 6.0, Tolerance) &&
		FMath::IsNearlyEqual(Sum.Y, 9.0, Tolerance));

	FVector2D Diff = V2DOps::Sub(B, A);
	TestTrue(TEXT("Sub"), FMath::IsNearlyEqual(Diff.X, 2.0, Tolerance) &&
		FMath::IsNearlyEqual(Diff.Y, 3.0, Tolerance));

	FVector2D Prod = V2DOps::Mult(A, B);
	TestTrue(TEXT("Mult"), FMath::IsNearlyEqual(Prod.X, 8.0, Tolerance) &&
		FMath::IsNearlyEqual(Prod.Y, 18.0, Tolerance));

	FVector2D Quot = V2DOps::Div(B, 2.0);
	TestTrue(TEXT("Div"), FMath::IsNearlyEqual(Quot.X, 2.0, Tolerance) &&
		FMath::IsNearlyEqual(Quot.Y, 3.0, Tolerance));

	// Lerp
	FVector2D Lerped = V2DOps::Lerp(A, B, 0.5);
	TestTrue(TEXT("Lerp 0.5"), FMath::IsNearlyEqual(Lerped.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(Lerped.Y, 4.5, Tolerance));

	// Min/Max (component-wise)
	FVector2D C(5, 1);
	FVector2D D(1, 5);
	FVector2D MinV = V2DOps::Min(C, D);
	TestTrue(TEXT("Min component-wise"),
		FMath::IsNearlyEqual(MinV.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(MinV.Y, 1.0, Tolerance));

	FVector2D MaxV = V2DOps::Max(C, D);
	TestTrue(TEXT("Max component-wise"),
		FMath::IsNearlyEqual(MaxV.X, 5.0, Tolerance) &&
		FMath::IsNearlyEqual(MaxV.Y, 5.0, Tolerance));

	// Average
	FVector2D Avg = V2DOps::Average(A, B);
	TestTrue(TEXT("Average"), FMath::IsNearlyEqual(Avg.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(Avg.Y, 4.5, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector2DAbsoluteOpsTest,
	"PCGEx.Unit.Types.TypeOps.Vector2D.AbsoluteOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector2DAbsoluteOpsTest::RunTest(const FString& Parameters)
{
	using V2DOps = PCGExTypeOps::FTypeOps<FVector2D>;
	const double Tolerance = 0.001;

	FVector2D A(3, -4);
	FVector2D B(-5, 2);

	// Abs
	FVector2D AbsA = V2DOps::Abs(A);
	TestTrue(TEXT("Abs"), FMath::IsNearlyEqual(AbsA.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(AbsA.Y, 4.0, Tolerance));

	// AbsoluteMin/Max
	FVector2D AbsMin = V2DOps::AbsoluteMin(A, B);
	TestTrue(TEXT("AbsoluteMin"),
		FMath::IsNearlyEqual(AbsMin.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(AbsMin.Y, 2.0, Tolerance));

	FVector2D AbsMax = V2DOps::AbsoluteMax(A, B);
	TestTrue(TEXT("AbsoluteMax"),
		FMath::IsNearlyEqual(AbsMax.X, 5.0, Tolerance) &&
		FMath::IsNearlyEqual(AbsMax.Y, 4.0, Tolerance));

	return true;
}

// =============================================================================
// FVector Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorDefaultTest,
	"PCGEx.Unit.Types.TypeOps.Vector.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorDefaultTest::RunTest(const FString& Parameters)
{
	FVector Default = PCGExTypeOps::FTypeOps<FVector>::GetDefault();
	TestTrue(TEXT("FVector default is zero"),
		PCGExTest::NearlyEqual(Default, FVector::ZeroVector, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Vector.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorConversionsTest::RunTest(const FString& Parameters)
{
	using VOps = PCGExTypeOps::FTypeOps<FVector>;
	const double Tolerance = 0.001;

	FVector V(1, 2, 3);

	// To scalar (extracts X)
	TestTrue(TEXT("FVector -> double extracts X"),
		FMath::IsNearlyEqual(VOps::ConvertTo<double>(V), 1.0, Tolerance));

	// To bool
	TestTrue(TEXT("Non-zero FVector -> true"), VOps::ConvertTo<bool>(V));
	TestFalse(TEXT("Zero FVector -> false"),
		VOps::ConvertTo<bool>(FVector::ZeroVector));

	// To Vector2D
	FVector2D AsV2 = VOps::ConvertTo<FVector2D>(V);
	TestTrue(TEXT("FVector -> Vector2D"),
		FMath::IsNearlyEqual(AsV2.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV2.Y, 2.0, Tolerance));

	// To Vector4
	FVector4 AsV4 = VOps::ConvertTo<FVector4>(V);
	TestTrue(TEXT("FVector -> Vector4"),
		FMath::IsNearlyEqual(AsV4.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV4.Y, 2.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV4.Z, 3.0, Tolerance));

	// From scalar
	FVector FromDouble = VOps::ConvertFrom<double>(5.0);
	TestTrue(TEXT("double -> FVector"),
		PCGExTest::NearlyEqual(FromDouble, FVector(5.0), Tolerance));

	// From Transform (extracts location)
	FTransform T(FQuat::Identity, FVector(10, 20, 30));
	FVector FromT = VOps::ConvertFrom<FTransform>(T);
	TestTrue(TEXT("FTransform -> FVector (location)"),
		PCGExTest::NearlyEqual(FromT, FVector(10, 20, 30), Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorBlendTest,
	"PCGEx.Unit.Types.TypeOps.Vector.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorBlendTest::RunTest(const FString& Parameters)
{
	using VOps = PCGExTypeOps::FTypeOps<FVector>;
	const double Tolerance = 0.001;

	FVector A(1, 2, 3);
	FVector B(4, 5, 6);

	// Basic arithmetic
	FVector Sum = VOps::Add(A, B);
	TestTrue(TEXT("Add"), PCGExTest::NearlyEqual(Sum, FVector(5, 7, 9), Tolerance));

	FVector Diff = VOps::Sub(B, A);
	TestTrue(TEXT("Sub"), PCGExTest::NearlyEqual(Diff, FVector(3, 3, 3), Tolerance));

	FVector Prod = VOps::Mult(A, B);
	TestTrue(TEXT("Mult"), PCGExTest::NearlyEqual(Prod, FVector(4, 10, 18), Tolerance));

	FVector Quot = VOps::Div(B, 2.0);
	TestTrue(TEXT("Div"), PCGExTest::NearlyEqual(Quot, FVector(2, 2.5, 3), Tolerance));

	// Lerp
	FVector Lerped = VOps::Lerp(A, B, 0.5);
	TestTrue(TEXT("Lerp 0.5"), PCGExTest::NearlyEqual(Lerped, FVector(2.5, 3.5, 4.5), Tolerance));

	// Min/Max
	FVector C(5, 1, 3);
	FVector D(1, 5, 3);
	FVector MinV = VOps::Min(C, D);
	TestTrue(TEXT("Min"), PCGExTest::NearlyEqual(MinV, FVector(1, 1, 3), Tolerance));

	FVector MaxV = VOps::Max(C, D);
	TestTrue(TEXT("Max"), PCGExTest::NearlyEqual(MaxV, FVector(5, 5, 3), Tolerance));

	// Average
	FVector Avg = VOps::Average(A, B);
	TestTrue(TEXT("Average"), PCGExTest::NearlyEqual(Avg, FVector(2.5, 3.5, 4.5), Tolerance));

	// Weighted operations
	FVector WAdd = VOps::WeightedAdd(A, B, 0.5);
	TestTrue(TEXT("WeightedAdd"), PCGExTest::NearlyEqual(WAdd, FVector(3, 4.5, 6), Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorModuloTest,
	"PCGEx.Unit.Types.TypeOps.Vector.Modulo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorModuloTest::RunTest(const FString& Parameters)
{
	using VOps = PCGExTypeOps::FTypeOps<FVector>;
	const double Tolerance = 0.001;

	FVector A(10, 15, 20);

	// ModSimple (same modulo for all)
	FVector ModS = VOps::ModSimple(A, 3.0);
	TestTrue(TEXT("ModSimple"),
		FMath::IsNearlyEqual(ModS.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(ModS.Y, 0.0, Tolerance) &&
		FMath::IsNearlyEqual(ModS.Z, 2.0, Tolerance));

	// ModComplex (different modulo per component)
	FVector B(3, 4, 7);
	FVector ModC = VOps::ModComplex(A, B);
	TestTrue(TEXT("ModComplex"),
		FMath::IsNearlyEqual(ModC.X, 1.0, Tolerance) &&  // 10 % 3 = 1
		FMath::IsNearlyEqual(ModC.Y, 3.0, Tolerance) &&  // 15 % 4 = 3
		FMath::IsNearlyEqual(ModC.Z, 6.0, Tolerance));   // 20 % 7 = 6

	// Division by zero protection
	FVector ZeroDiv = VOps::ModComplex(A, FVector(0, 4, 0));
	TestTrue(TEXT("ModComplex div by zero protection"),
		FMath::IsNearlyEqual(ZeroDiv.X, 10.0, Tolerance) &&  // Returns original
		FMath::IsNearlyEqual(ZeroDiv.Y, 3.0, Tolerance) &&   // Normal mod
		FMath::IsNearlyEqual(ZeroDiv.Z, 20.0, Tolerance));   // Returns original

	return true;
}

// =============================================================================
// FVector4 Type Operations Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector4DefaultTest,
	"PCGEx.Unit.Types.TypeOps.Vector4.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector4DefaultTest::RunTest(const FString& Parameters)
{
	FVector4 Default = PCGExTypeOps::FTypeOps<FVector4>::GetDefault();
	TestTrue(TEXT("FVector4 default is zero"),
		FMath::IsNearlyZero(Default.X) &&
		FMath::IsNearlyZero(Default.Y) &&
		FMath::IsNearlyZero(Default.Z) &&
		FMath::IsNearlyZero(Default.W));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector4BlendTest,
	"PCGEx.Unit.Types.TypeOps.Vector4.Blend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector4BlendTest::RunTest(const FString& Parameters)
{
	using V4Ops = PCGExTypeOps::FTypeOps<FVector4>;
	const double Tolerance = 0.001;

	FVector4 A(1, 2, 3, 4);
	FVector4 B(5, 6, 7, 8);

	// Add
	FVector4 Sum = V4Ops::Add(A, B);
	TestTrue(TEXT("Add"),
		FMath::IsNearlyEqual(Sum.X, 6.0, Tolerance) &&
		FMath::IsNearlyEqual(Sum.Y, 8.0, Tolerance) &&
		FMath::IsNearlyEqual(Sum.Z, 10.0, Tolerance) &&
		FMath::IsNearlyEqual(Sum.W, 12.0, Tolerance));

	// Lerp
	FVector4 Lerped = V4Ops::Lerp(A, B, 0.5);
	TestTrue(TEXT("Lerp 0.5"),
		FMath::IsNearlyEqual(Lerped.X, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(Lerped.Y, 4.0, Tolerance) &&
		FMath::IsNearlyEqual(Lerped.Z, 5.0, Tolerance) &&
		FMath::IsNearlyEqual(Lerped.W, 6.0, Tolerance));

	// Min/Max
	FVector4 C(5, 1, 3, 7);
	FVector4 D(1, 5, 3, 2);
	FVector4 MinV = V4Ops::Min(C, D);
	TestTrue(TEXT("Min"),
		FMath::IsNearlyEqual(MinV.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(MinV.Y, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(MinV.Z, 3.0, Tolerance) &&
		FMath::IsNearlyEqual(MinV.W, 2.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVector4ConversionsTest,
	"PCGEx.Unit.Types.TypeOps.Vector4.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVector4ConversionsTest::RunTest(const FString& Parameters)
{
	using V4Ops = PCGExTypeOps::FTypeOps<FVector4>;
	const double Tolerance = 0.001;

	FVector4 V(1, 2, 3, 4);

	// To FVector (drops W)
	FVector AsV3 = V4Ops::ConvertTo<FVector>(V);
	TestTrue(TEXT("Vector4 -> FVector"),
		FMath::IsNearlyEqual(AsV3.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV3.Y, 2.0, Tolerance) &&
		FMath::IsNearlyEqual(AsV3.Z, 3.0, Tolerance));

	// To scalar
	TestTrue(TEXT("Vector4 -> double extracts X"),
		FMath::IsNearlyEqual(V4Ops::ConvertTo<double>(V), 1.0, Tolerance));

	// From FVector
	FVector V3(5, 6, 7);
	FVector4 FromV3 = V4Ops::ConvertFrom<FVector>(V3);
	TestTrue(TEXT("FVector -> Vector4"),
		FMath::IsNearlyEqual(FromV3.X, 5.0, Tolerance) &&
		FMath::IsNearlyEqual(FromV3.Y, 6.0, Tolerance) &&
		FMath::IsNearlyEqual(FromV3.Z, 7.0, Tolerance));

	return true;
}

// =============================================================================
// Cross-Type Round Trip Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorRoundTripTest,
	"PCGEx.Unit.Types.TypeOps.Vector.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorRoundTripTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.001;

	// FVector -> FVector2D -> FVector (Z lost)
	FVector Orig(1, 2, 3);
	FVector2D AsV2 = PCGExTypeOps::FTypeOps<FVector>::ConvertTo<FVector2D>(Orig);
	FVector BackV = PCGExTypeOps::FTypeOps<FVector2D>::ConvertTo<FVector>(AsV2);
	TestTrue(TEXT("FVector -> FVector2D -> FVector (Z becomes 0)"),
		FMath::IsNearlyEqual(BackV.X, 1.0, Tolerance) &&
		FMath::IsNearlyEqual(BackV.Y, 2.0, Tolerance) &&
		FMath::IsNearlyZero(BackV.Z, Tolerance));

	// FVector -> FVector4 -> FVector
	FVector4 AsV4 = PCGExTypeOps::FTypeOps<FVector>::ConvertTo<FVector4>(Orig);
	FVector BackV2 = PCGExTypeOps::FTypeOps<FVector4>::ConvertTo<FVector>(AsV4);
	TestTrue(TEXT("FVector -> FVector4 -> FVector preserves XYZ"),
		PCGExTest::NearlyEqual(BackV2, Orig, Tolerance));

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTypeOpsVectorEdgeCasesTest,
	"PCGEx.Unit.Types.TypeOps.Vector.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTypeOpsVectorEdgeCasesTest::RunTest(const FString& Parameters)
{
	using VOps = PCGExTypeOps::FTypeOps<FVector>;
	const double Tolerance = 0.001;

	// Division by zero
	FVector V(10, 20, 30);
	FVector DivZero = VOps::Div(V, 0.0);
	TestTrue(TEXT("Div by zero returns original"),
		PCGExTest::NearlyEqual(DivZero, V, Tolerance));

	// ModSimple with zero
	FVector ModZero = VOps::ModSimple(V, 0.0);
	TestTrue(TEXT("ModSimple by zero returns original"),
		PCGExTest::NearlyEqual(ModZero, V, Tolerance));

	// Normalize weight by zero
	FVector NormZero = VOps::NormalizeWeight(V, 0.0);
	TestTrue(TEXT("NormalizeWeight by zero returns original"),
		PCGExTest::NearlyEqual(NormZero, V, Tolerance));

	// Factor with zero
	FVector FactorZero = VOps::Factor(V, 0.0);
	TestTrue(TEXT("Factor by zero returns zero vector"),
		PCGExTest::NearlyEqual(FactorZero, FVector::ZeroVector, Tolerance));

	return true;
}
