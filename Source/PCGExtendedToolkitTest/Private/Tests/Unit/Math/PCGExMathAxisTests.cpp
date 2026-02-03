// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMathAxis Unit Tests
 *
 * Tests axis manipulation functions from PCGExMathAxis.h:
 * - GetAxesOrder: Retrieve axis order indices
 * - ReorderAxes: Reorder vectors by axis order
 * - GetDirection: Get direction vector from quaternion
 * - Swizzle: Swizzle vector components
 * - Angle functions: GetAngle, GetRadiansBetweenVectors, GetDegreesBetweenVectors
 *
 * Test naming convention: PCGEx.Unit.Math.Axis.<FunctionName>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExMathAxis.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// GetAxesOrder Tests
// =============================================================================

/**
 * Test GetAxesOrder with EPCGExAxisOrder enum
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathAxisOrderTest,
	"PCGEx.Unit.Math.Axis.GetAxesOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathAxisOrderTest::RunTest(const FString& Parameters)
{
	int32 A, B, C;

	// XYZ order (default)
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::XYZ, A, B, C);
	TestEqual(TEXT("XYZ order: A=0"), A, 0);
	TestEqual(TEXT("XYZ order: B=1"), B, 1);
	TestEqual(TEXT("XYZ order: C=2"), C, 2);

	// YZX order
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::YZX, A, B, C);
	TestEqual(TEXT("YZX order: A=1"), A, 1);
	TestEqual(TEXT("YZX order: B=2"), B, 2);
	TestEqual(TEXT("YZX order: C=0"), C, 0);

	// ZXY order
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::ZXY, A, B, C);
	TestEqual(TEXT("ZXY order: A=2"), A, 2);
	TestEqual(TEXT("ZXY order: B=0"), B, 0);
	TestEqual(TEXT("ZXY order: C=1"), C, 1);

	// YXZ order
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::YXZ, A, B, C);
	TestEqual(TEXT("YXZ order: A=1"), A, 1);
	TestEqual(TEXT("YXZ order: B=0"), B, 0);
	TestEqual(TEXT("YXZ order: C=2"), C, 2);

	// ZYX order
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::ZYX, A, B, C);
	TestEqual(TEXT("ZYX order: A=2"), A, 2);
	TestEqual(TEXT("ZYX order: B=1"), B, 1);
	TestEqual(TEXT("ZYX order: C=0"), C, 0);

	// XZY order
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::XZY, A, B, C);
	TestEqual(TEXT("XZY order: A=0"), A, 0);
	TestEqual(TEXT("XZY order: B=2"), B, 2);
	TestEqual(TEXT("XZY order: C=1"), C, 1);

	// Test array version
	int32 Order[3];
	PCGExMath::GetAxesOrder(EPCGExAxisOrder::ZYX, Order);
	TestEqual(TEXT("ZYX array[0]=2"), Order[0], 2);
	TestEqual(TEXT("ZYX array[1]=1"), Order[1], 1);
	TestEqual(TEXT("ZYX array[2]=0"), Order[2], 0);

	return true;
}

// =============================================================================
// ReorderAxes Tests
// =============================================================================

/**
 * Test ReorderAxes function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathReorderAxesTest,
	"PCGEx.Unit.Math.Axis.ReorderAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathReorderAxesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Test XYZ (identity - no reorder)
	{
		FVector X(1, 0, 0);
		FVector Y(0, 1, 0);
		FVector Z(0, 0, 1);
		PCGExMath::ReorderAxes(EPCGExAxisOrder::XYZ, X, Y, Z);
		TestTrue(TEXT("XYZ: X unchanged"), PCGExTest::NearlyEqual(X, FVector(1, 0, 0), Tolerance));
		TestTrue(TEXT("XYZ: Y unchanged"), PCGExTest::NearlyEqual(Y, FVector(0, 1, 0), Tolerance));
		TestTrue(TEXT("XYZ: Z unchanged"), PCGExTest::NearlyEqual(Z, FVector(0, 0, 1), Tolerance));
	}

	// Test YZX order (shift left)
	{
		FVector X(1, 0, 0);
		FVector Y(0, 1, 0);
		FVector Z(0, 0, 1);
		PCGExMath::ReorderAxes(EPCGExAxisOrder::YZX, X, Y, Z);
		// Y becomes first (X position), Z becomes second (Y position), X becomes third (Z position)
		TestTrue(TEXT("YZX: X becomes Y"), PCGExTest::NearlyEqual(X, FVector(0, 1, 0), Tolerance));
		TestTrue(TEXT("YZX: Y becomes Z"), PCGExTest::NearlyEqual(Y, FVector(0, 0, 1), Tolerance));
		TestTrue(TEXT("YZX: Z becomes X"), PCGExTest::NearlyEqual(Z, FVector(1, 0, 0), Tolerance));
	}

	// Test ZYX order (reverse)
	{
		FVector X(1, 0, 0);
		FVector Y(0, 1, 0);
		FVector Z(0, 0, 1);
		PCGExMath::ReorderAxes(EPCGExAxisOrder::ZYX, X, Y, Z);
		TestTrue(TEXT("ZYX: X becomes Z"), PCGExTest::NearlyEqual(X, FVector(0, 0, 1), Tolerance));
		TestTrue(TEXT("ZYX: Y unchanged"), PCGExTest::NearlyEqual(Y, FVector(0, 1, 0), Tolerance));
		TestTrue(TEXT("ZYX: Z becomes X"), PCGExTest::NearlyEqual(Z, FVector(1, 0, 0), Tolerance));
	}

	return true;
}

// =============================================================================
// GetDirection Tests
// =============================================================================

/**
 * Test GetDirection with identity quaternion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetDirectionIdentityTest,
	"PCGEx.Unit.Math.Axis.GetDirection.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetDirectionIdentityTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;
	const FQuat Identity = FQuat::Identity;

	// Test compile-time templated version
	TestTrue(TEXT("Identity Forward = (1,0,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Forward>(Identity),
		         FVector::ForwardVector, Tolerance));

	TestTrue(TEXT("Identity Backward = (-1,0,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Backward>(Identity),
		         FVector::BackwardVector, Tolerance));

	TestTrue(TEXT("Identity Right = (0,1,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Right>(Identity),
		         FVector::RightVector, Tolerance));

	TestTrue(TEXT("Identity Left = (0,-1,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Left>(Identity),
		         FVector::LeftVector, Tolerance));

	TestTrue(TEXT("Identity Up = (0,0,1)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Up>(Identity),
		         FVector::UpVector, Tolerance));

	TestTrue(TEXT("Identity Down = (0,0,-1)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Down>(Identity),
		         FVector::DownVector, Tolerance));

	return true;
}

/**
 * Test GetDirection with rotated quaternion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetDirectionRotatedTest,
	"PCGEx.Unit.Math.Axis.GetDirection.Rotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetDirectionRotatedTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.001;

	// 90 degree rotation around Z axis
	const FQuat RotZ90 = FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0));

	// Forward should now point to Right (Y+)
	TestTrue(TEXT("RotZ90 Forward ~ (0,1,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Forward>(RotZ90),
		         FVector::RightVector, Tolerance));

	// Right should now point to Backward (X-)
	TestTrue(TEXT("RotZ90 Right ~ (-1,0,0)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Right>(RotZ90),
		         FVector::BackwardVector, Tolerance));

	// Up should still be Up
	TestTrue(TEXT("RotZ90 Up ~ (0,0,1)"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection<EPCGExAxis::Up>(RotZ90),
		         FVector::UpVector, Tolerance));

	return true;
}

/**
 * Test runtime dispatch version of GetDirection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetDirectionRuntimeTest,
	"PCGEx.Unit.Math.Axis.GetDirection.Runtime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetDirectionRuntimeTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;
	const FQuat Identity = FQuat::Identity;

	// Test runtime dispatch matches compile-time
	TestTrue(TEXT("Runtime Forward matches template"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection(Identity, EPCGExAxis::Forward),
		         PCGExMath::GetDirection<EPCGExAxis::Forward>(Identity), Tolerance));

	TestTrue(TEXT("Runtime Right matches template"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection(Identity, EPCGExAxis::Right),
		         PCGExMath::GetDirection<EPCGExAxis::Right>(Identity), Tolerance));

	TestTrue(TEXT("Runtime Up matches template"),
	         PCGExTest::NearlyEqual(
		         PCGExMath::GetDirection(Identity, EPCGExAxis::Up),
		         PCGExMath::GetDirection<EPCGExAxis::Up>(Identity), Tolerance));

	return true;
}

/**
 * Test GetDirection with axis only (no quaternion)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetDirectionAxisOnlyTest,
	"PCGEx.Unit.Math.Axis.GetDirection.AxisOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetDirectionAxisOnlyTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	TestTrue(TEXT("Forward axis = (1,0,0)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Forward), FVector::ForwardVector, Tolerance));

	TestTrue(TEXT("Backward axis = (-1,0,0)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Backward), FVector::BackwardVector, Tolerance));

	TestTrue(TEXT("Right axis = (0,1,0)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Right), FVector::RightVector, Tolerance));

	TestTrue(TEXT("Left axis = (0,-1,0)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Left), FVector::LeftVector, Tolerance));

	TestTrue(TEXT("Up axis = (0,0,1)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Up), FVector::UpVector, Tolerance));

	TestTrue(TEXT("Down axis = (0,0,-1)"),
	         PCGExTest::NearlyEqual(PCGExMath::GetDirection(EPCGExAxis::Down), FVector::DownVector, Tolerance));

	return true;
}

// =============================================================================
// Swizzle Tests
// =============================================================================

/**
 * Test Swizzle function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathSwizzleTest,
	"PCGEx.Unit.Math.Axis.Swizzle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathSwizzleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// XYZ (identity - no change)
	{
		FVector V(1, 2, 3);
		PCGExMath::Swizzle(V, EPCGExAxisOrder::XYZ);
		TestTrue(TEXT("Swizzle XYZ: no change"), PCGExTest::NearlyEqual(V, FVector(1, 2, 3), Tolerance));
	}

	// YZX order
	{
		FVector V(1, 2, 3);
		PCGExMath::Swizzle(V, EPCGExAxisOrder::YZX);
		TestTrue(TEXT("Swizzle YZX: (1,2,3) -> (2,3,1)"), PCGExTest::NearlyEqual(V, FVector(2, 3, 1), Tolerance));
	}

	// ZXY order
	{
		FVector V(1, 2, 3);
		PCGExMath::Swizzle(V, EPCGExAxisOrder::ZXY);
		TestTrue(TEXT("Swizzle ZXY: (1,2,3) -> (3,1,2)"), PCGExTest::NearlyEqual(V, FVector(3, 1, 2), Tolerance));
	}

	// ZYX order (reverse)
	{
		FVector V(1, 2, 3);
		PCGExMath::Swizzle(V, EPCGExAxisOrder::ZYX);
		TestTrue(TEXT("Swizzle ZYX: (1,2,3) -> (3,2,1)"), PCGExTest::NearlyEqual(V, FVector(3, 2, 1), Tolerance));
	}

	// Test array version
	{
		FVector V(1, 2, 3);
		int32 Order[3] = {2, 0, 1}; // ZXY
		PCGExMath::Swizzle(V, Order);
		TestTrue(TEXT("Swizzle array ZXY: (1,2,3) -> (3,1,2)"), PCGExTest::NearlyEqual(V, FVector(3, 1, 2), Tolerance));
	}

	return true;
}

// =============================================================================
// GetNormal Tests
// =============================================================================

/**
 * Test GetNormal for computing triangle normal
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetNormalTest,
	"PCGEx.Unit.Math.Axis.GetNormal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetNormalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.001;

	// XY plane triangle - normal should point up (Z+)
	{
		FVector A(0, 0, 0);
		FVector B(1, 0, 0);
		FVector C(0, 1, 0);
		FVector Normal = PCGExMath::GetNormal(A, B, C);
		TestTrue(TEXT("XY plane normal ~ (0,0,1) or (0,0,-1)"),
		         FMath::Abs(FMath::Abs(Normal.Z) - 1.0) < Tolerance);
	}

	// XZ plane triangle - normal should point right or left (Y axis)
	{
		FVector A(0, 0, 0);
		FVector B(1, 0, 0);
		FVector C(0, 0, 1);
		FVector Normal = PCGExMath::GetNormal(A, B, C);
		TestTrue(TEXT("XZ plane normal ~ Y axis"),
		         FMath::Abs(FMath::Abs(Normal.Y) - 1.0) < Tolerance);
	}

	// YZ plane triangle - normal should point forward or backward (X axis)
	{
		FVector A(0, 0, 0);
		FVector B(0, 1, 0);
		FVector C(0, 0, 1);
		FVector Normal = PCGExMath::GetNormal(A, B, C);
		TestTrue(TEXT("YZ plane normal ~ X axis"),
		         FMath::Abs(FMath::Abs(Normal.X) - 1.0) < Tolerance);
	}

	return true;
}

/**
 * Test GetNormalUp for computing normal with up reference
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetNormalUpTest,
	"PCGEx.Unit.Math.Axis.GetNormalUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetNormalUpTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.001;

	// Line along X axis with Up reference
	{
		FVector A(0, 0, 0);
		FVector B(1, 0, 0);
		FVector Normal = PCGExMath::GetNormalUp(A, B, FVector::UpVector);
		// Normal should be perpendicular to both the line and up vector
		TestTrue(TEXT("Normal perpendicular to line"),
		         FMath::IsNearlyZero(FVector::DotProduct(Normal, (B - A).GetSafeNormal()), Tolerance));
	}

	return true;
}

// =============================================================================
// Angle Function Tests
// =============================================================================

/**
 * Test GetAngle function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetAngleTest,
	"PCGEx.Unit.Math.Axis.GetAngle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetAngleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// GetAngle returns radians, not degrees
	// Same direction - 0 radians
	TestTrue(TEXT("GetAngle(X, X) ~ 0"),
	         FMath::IsNearlyEqual(PCGExMath::GetAngle(FVector::ForwardVector, FVector::ForwardVector), 0.0, Tolerance));

	// Perpendicular - PI/2 radians
	TestTrue(TEXT("GetAngle(X, Y) ~ PI/2"),
	         FMath::IsNearlyEqual(PCGExMath::GetAngle(FVector::ForwardVector, FVector::RightVector), UE_HALF_PI, Tolerance));

	// Opposite direction - PI radians
	TestTrue(TEXT("GetAngle(X, -X) ~ PI"),
	         FMath::IsNearlyEqual(PCGExMath::GetAngle(FVector::ForwardVector, FVector::BackwardVector), UE_PI, Tolerance));

	// 45 degree angle - PI/4 radians
	FVector Diagonal = FVector(1, 1, 0).GetSafeNormal();
	TestTrue(TEXT("GetAngle(X, XY diagonal) ~ PI/4"),
	         FMath::IsNearlyEqual(PCGExMath::GetAngle(FVector::ForwardVector, Diagonal), UE_PI / 4.0, Tolerance));

	return true;
}

/**
 * Test GetRadiansBetweenVectors for 3D vectors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetRadiansBetweenVectors3DTest,
	"PCGEx.Unit.Math.Axis.GetRadiansBetweenVectors.3D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetRadiansBetweenVectors3DTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Same direction - 0 radians
	TestTrue(TEXT("Radians(X, X) ~ 0"),
	         FMath::IsNearlyEqual(
		         PCGExMath::GetRadiansBetweenVectors(FVector::ForwardVector, FVector::ForwardVector),
		         0.0, Tolerance));

	// Perpendicular - PI/2 radians
	TestTrue(TEXT("Radians(X, Y) ~ PI/2"),
	         FMath::IsNearlyEqual(
		         FMath::Abs(PCGExMath::GetRadiansBetweenVectors(FVector::ForwardVector, FVector::RightVector)),
		         UE_HALF_PI, Tolerance));

	return true;
}

/**
 * Test GetRadiansBetweenVectors for 2D vectors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetRadiansBetweenVectors2DTest,
	"PCGEx.Unit.Math.Axis.GetRadiansBetweenVectors.2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetRadiansBetweenVectors2DTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Same direction
	TestTrue(TEXT("Radians2D((1,0), (1,0)) ~ 0"),
	         FMath::IsNearlyEqual(
		         PCGExMath::GetRadiansBetweenVectors(FVector2D(1, 0), FVector2D(1, 0)),
		         0.0, Tolerance));

	// 90 degrees CCW
	TestTrue(TEXT("Radians2D((1,0), (0,1)) ~ PI/2"),
	         FMath::IsNearlyEqual(
		         PCGExMath::GetRadiansBetweenVectors(FVector2D(1, 0), FVector2D(0, 1)),
		         UE_HALF_PI, Tolerance));

	// 90 degrees CW - function returns [0, 2π), so CW 90° = 3π/2
	TestTrue(TEXT("Radians2D((1,0), (0,-1)) ~ 3*PI/2"),
	         FMath::IsNearlyEqual(
		         PCGExMath::GetRadiansBetweenVectors(FVector2D(1, 0), FVector2D(0, -1)),
		         UE_PI + UE_HALF_PI, Tolerance));

	return true;
}

/**
 * Test GetDegreesBetweenVectors
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExMathGetDegreesBetweenVectorsTest,
	"PCGEx.Unit.Math.Axis.GetDegreesBetweenVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExMathGetDegreesBetweenVectorsTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Same direction
	TestTrue(TEXT("Degrees(X, X) ~ 0"),
	         FMath::IsNearlyEqual(
		         PCGExMath::GetDegreesBetweenVectors(FVector::ForwardVector, FVector::ForwardVector),
		         0.0, Tolerance));

	// Perpendicular
	TestTrue(TEXT("Degrees(X, Y) ~ +/-90"),
	         FMath::IsNearlyEqual(
		         FMath::Abs(PCGExMath::GetDegreesBetweenVectors(FVector::ForwardVector, FVector::RightVector)),
		         90.0, Tolerance));

	return true;
}
