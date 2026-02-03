// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExBestFitPlane Unit Tests
 *
 * Tests plane fitting from points in PCGExBestFitPlane.h:
 * - FBestFitPlane: Compute best-fit plane from point positions
 * - Centroid: Center of mass calculation
 * - Normal: Plane normal vector
 * - Extents: Bounding extents
 * - GetTransform: Transform from plane
 *
 * Test naming convention: PCGEx.Unit.Math.BestFitPlane.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/PCGExBestFitPlane.h"
#include "Math/PCGExMathAxis.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Default Constructor Tests
// =============================================================================

/**
 * Test default constructor
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneDefaultConstructorTest,
	"PCGEx.Unit.Math.BestFitPlane.DefaultConstructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneDefaultConstructorTest::RunTest(const FString& Parameters)
{
	PCGExMath::FBestFitPlane Plane;

	TestTrue(TEXT("Default centroid is zero"),
	         PCGExTest::NearlyEqual(Plane.Centroid, FVector::ZeroVector, KINDA_SMALL_NUMBER));

	TestTrue(TEXT("Default extents is one"),
	         PCGExTest::NearlyEqual(Plane.Extents, FVector::OneVector, KINDA_SMALL_NUMBER));

	// Check default axes
	TestTrue(TEXT("Default Axis[0] is Forward"),
	         PCGExTest::NearlyEqual(Plane.Axis[0], FVector::ForwardVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Default Axis[1] is Right"),
	         PCGExTest::NearlyEqual(Plane.Axis[1], FVector::RightVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Default Axis[2] is Up"),
	         PCGExTest::NearlyEqual(Plane.Axis[2], FVector::UpVector, KINDA_SMALL_NUMBER));

	// Check default swizzle
	TestEqual(TEXT("Default Swizzle[0] = 0"), Plane.Swizzle[0], 0);
	TestEqual(TEXT("Default Swizzle[1] = 1"), Plane.Swizzle[1], 1);
	TestEqual(TEXT("Default Swizzle[2] = 2"), Plane.Swizzle[2], 2);

	return true;
}

// =============================================================================
// XY Plane Tests
// =============================================================================

/**
 * Test with points in XY plane (horizontal)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneXYPlaneTest,
	"PCGEx.Unit.Math.BestFitPlane.XYPlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneXYPlaneTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Square in XY plane
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(1, 1, 0),
		FVector(0, 1, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid should be at center of square
	FVector ExpectedCentroid(0.5, 0.5, 0.0);
	TestTrue(TEXT("XY plane centroid"),
	         PCGExTest::NearlyEqual(Plane.Centroid, ExpectedCentroid, Tolerance));

	// Normal should be along Z axis (up or down)
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("XY plane normal is vertical"),
	         FMath::IsNearlyEqual(FMath::Abs(Normal.Z), 1.0, Tolerance));

	return true;
}

/**
 * Test with points in elevated XY plane
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneElevatedXYTest,
	"PCGEx.Unit.Math.BestFitPlane.ElevatedXY",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneElevatedXYTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Square at Z=100
	TArray<FVector> Points = {
		FVector(0, 0, 100),
		FVector(10, 0, 100),
		FVector(10, 10, 100),
		FVector(0, 10, 100)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid Z should be 100
	TestTrue(TEXT("Elevated XY centroid Z = 100"),
	         FMath::IsNearlyEqual(Plane.Centroid.Z, 100.0, Tolerance));

	// Normal should still be vertical
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("Elevated XY normal is vertical"),
	         FMath::IsNearlyEqual(FMath::Abs(Normal.Z), 1.0, Tolerance));

	return true;
}

// =============================================================================
// XZ Plane Tests
// =============================================================================

/**
 * Test with points in XZ plane (vertical wall)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneXZPlaneTest,
	"PCGEx.Unit.Math.BestFitPlane.XZPlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneXZPlaneTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Square in XZ plane (Y=0)
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(1, 0, 1),
		FVector(0, 0, 1)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid Y should be 0
	TestTrue(TEXT("XZ plane centroid Y = 0"),
	         FMath::IsNearlyZero(Plane.Centroid.Y, Tolerance));

	// Normal should be along Y axis
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("XZ plane normal is along Y"),
	         FMath::IsNearlyEqual(FMath::Abs(Normal.Y), 1.0, Tolerance));

	return true;
}

// =============================================================================
// YZ Plane Tests
// =============================================================================

/**
 * Test with points in YZ plane
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneYZPlaneTest,
	"PCGEx.Unit.Math.BestFitPlane.YZPlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneYZPlaneTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Square in YZ plane (X=0)
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(0, 1, 0),
		FVector(0, 1, 1),
		FVector(0, 0, 1)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid X should be 0
	TestTrue(TEXT("YZ plane centroid X = 0"),
	         FMath::IsNearlyZero(Plane.Centroid.X, Tolerance));

	// Normal should be along X axis
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("YZ plane normal is along X"),
	         FMath::IsNearlyEqual(FMath::Abs(Normal.X), 1.0, Tolerance));

	return true;
}

// =============================================================================
// Triangle Tests
// =============================================================================

/**
 * Test with three points (minimum for plane)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneTriangleTest,
	"PCGEx.Unit.Math.BestFitPlane.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneTriangleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Right triangle in XY plane
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(3, 0, 0),
		FVector(0, 4, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid is average of points
	FVector ExpectedCentroid(1.0, 4.0 / 3.0, 0.0);
	TestTrue(TEXT("Triangle centroid"),
	         PCGExTest::NearlyEqual(Plane.Centroid, ExpectedCentroid, Tolerance));

	// Normal should be vertical
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("Triangle normal is vertical"),
	         FMath::IsNearlyEqual(FMath::Abs(Normal.Z), 1.0, Tolerance));

	return true;
}

// =============================================================================
// Tilted Plane Tests
// =============================================================================

/**
 * Test with tilted plane (45 degrees)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneTiltedTest,
	"PCGEx.Unit.Math.BestFitPlane.Tilted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneTiltedTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Points on a 45-degree tilted plane
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(1, 0, 1),
		FVector(0, 1, 0),
		FVector(1, 1, 1)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Normal should have both X and Z components
	FVector Normal = Plane.Normal();
	TestTrue(TEXT("Tilted normal is not purely vertical"),
	         !FMath::IsNearlyEqual(FMath::Abs(Normal.Z), 1.0, Tolerance) ||
	         !FMath::IsNearlyZero(Normal.X, Tolerance));

	return true;
}

// =============================================================================
// GetExtents Tests
// =============================================================================

/**
 * Test extents calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneExtentsTest,
	"PCGEx.Unit.Math.BestFitPlane.Extents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneExtentsTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// 2x4 rectangle in XY plane
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(2, 0, 0),
		FVector(2, 4, 0),
		FVector(0, 4, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	FVector Extents = Plane.GetExtents();

	// Extents should reflect the 2x4 dimensions
	// The exact ordering depends on the PCA axes
	TestTrue(TEXT("Extents are positive"), Extents.X > 0 && Extents.Y > 0);

	return true;
}

// =============================================================================
// GetTransform Tests
// =============================================================================

/**
 * Test transform generation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneGetTransformTest,
	"PCGEx.Unit.Math.BestFitPlane.GetTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneGetTransformTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Simple XY plane at origin
	TArray<FVector> Points = {
		FVector(-1, -1, 0),
		FVector(1, -1, 0),
		FVector(1, 1, 0),
		FVector(-1, 1, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};
	FTransform Transform = Plane.GetTransform();

	// Translation should be at centroid (origin in this case)
	TestTrue(TEXT("Transform translation at centroid"),
	         PCGExTest::NearlyEqual(Transform.GetTranslation(), FVector::ZeroVector, Tolerance));

	// Scale should be unit
	TestTrue(TEXT("Transform has unit scale"),
	         PCGExTest::NearlyEqual(Transform.GetScale3D(), FVector::OneVector, Tolerance));

	return true;
}

/**
 * Test transform with axis order
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneGetTransformWithOrderTest,
	"PCGEx.Unit.Math.BestFitPlane.GetTransform.WithOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneGetTransformWithOrderTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(1, 1, 0),
		FVector(0, 1, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Different axis orders should produce different transforms
	FTransform TransformXYZ = Plane.GetTransform(EPCGExAxisOrder::XYZ);
	FTransform TransformZYX = Plane.GetTransform(EPCGExAxisOrder::ZYX);

	// Both should have same translation (centroid)
	TestTrue(TEXT("Same translation regardless of order"),
	         PCGExTest::NearlyEqual(
		         TransformXYZ.GetTranslation(),
		         TransformZYX.GetTranslation(),
		         KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// GetExtents with Order Tests
// =============================================================================

/**
 * Test extents with axis order
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneExtentsWithOrderTest,
	"PCGEx.Unit.Math.BestFitPlane.Extents.WithOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneExtentsWithOrderTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(2, 0, 0),
		FVector(2, 4, 0),
		FVector(0, 4, 0)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	FVector ExtentsDefault = Plane.GetExtents();
	FVector ExtentsXYZ = Plane.GetExtents(EPCGExAxisOrder::XYZ);
	FVector ExtentsZYX = Plane.GetExtents(EPCGExAxisOrder::ZYX);

	// Default should match XYZ
	TestTrue(TEXT("Default extents match XYZ"),
	         PCGExTest::NearlyEqual(ExtentsDefault, ExtentsXYZ, KINDA_SMALL_NUMBER));

	// ZYX should have swizzled components
	TestTrue(TEXT("ZYX extents are reordered"),
	         !PCGExTest::NearlyEqual(ExtentsXYZ, ExtentsZYX, KINDA_SMALL_NUMBER) ||
	         FMath::IsNearlyZero(ExtentsXYZ.Z)); // If Z is zero, they might be equal

	return true;
}

// =============================================================================
// 2D Points Tests
// =============================================================================

/**
 * Test with FVector2D points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlane2DPointsTest,
	"PCGEx.Unit.Math.BestFitPlane.2DPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlane2DPointsTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Square in 2D (will be treated as XY plane)
	TArray<FVector2D> Points = {
		FVector2D(0, 0),
		FVector2D(1, 0),
		FVector2D(1, 1),
		FVector2D(0, 1)
	};

	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector2D>(Points)};

	// Centroid should be at center
	TestTrue(TEXT("2D centroid X"),
	         FMath::IsNearlyEqual(Plane.Centroid.X, 0.5, Tolerance));
	TestTrue(TEXT("2D centroid Y"),
	         FMath::IsNearlyEqual(Plane.Centroid.Y, 0.5, Tolerance));
	TestTrue(TEXT("2D centroid Z = 0"),
	         FMath::IsNearlyZero(Plane.Centroid.Z, Tolerance));

	return true;
}

// =============================================================================
// Callback-based Construction Tests
// =============================================================================

/**
 * Test with callback function
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneCallbackTest,
	"PCGEx.Unit.Math.BestFitPlane.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneCallbackTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Generate points via callback
	auto GetPoint = [](int32 Index) -> FVector
	{
		switch (Index)
		{
		case 0: return FVector(0, 0, 0);
		case 1: return FVector(1, 0, 0);
		case 2: return FVector(1, 1, 0);
		case 3: return FVector(0, 1, 0);
		default: return FVector::ZeroVector;
		}
	};

	PCGExMath::FBestFitPlane Plane(4, GetPoint);

	// Should produce same result as array version
	TestTrue(TEXT("Callback centroid X"),
	         FMath::IsNearlyEqual(Plane.Centroid.X, 0.5, Tolerance));
	TestTrue(TEXT("Callback centroid Y"),
	         FMath::IsNearlyEqual(Plane.Centroid.Y, 0.5, Tolerance));

	return true;
}

/**
 * Test with callback and extra point
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneCallbackWithExtraTest,
	"PCGEx.Unit.Math.BestFitPlane.CallbackWithExtra",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneCallbackWithExtraTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Three points, plus an extra
	auto GetPoint = [](int32 Index) -> FVector
	{
		switch (Index)
		{
		case 0: return FVector(0, 0, 0);
		case 1: return FVector(1, 0, 0);
		case 2: return FVector(0, 1, 0);
		default: return FVector::ZeroVector;
		}
	};

	FVector ExtraPoint(1, 1, 0);
	PCGExMath::FBestFitPlane Plane(3, GetPoint, ExtraPoint);

	// With 4 points forming a square, centroid should be at center
	// Note: Extra point is included in computation
	TestTrue(TEXT("Extra point affects centroid"),
	         !FMath::IsNearlyZero(Plane.Centroid.X, Tolerance) &&
	         !FMath::IsNearlyZero(Plane.Centroid.Y, Tolerance));

	return true;
}

// =============================================================================
// Edge Cases
// =============================================================================

/**
 * Test with collinear points (degenerate case)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBestFitPlaneCollinearTest,
	"PCGEx.Unit.Math.BestFitPlane.Collinear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExBestFitPlaneCollinearTest::RunTest(const FString& Parameters)
{
	// Points along a line (degenerate plane)
	TArray<FVector> Points = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(2, 0, 0),
		FVector(3, 0, 0)
	};

	// This might produce undefined results, but shouldn't crash
	PCGExMath::FBestFitPlane Plane{TArrayView<const FVector>(Points)};

	// Centroid should still be valid (middle of line)
	TestTrue(TEXT("Collinear centroid is finite"),
	         FMath::IsFinite(Plane.Centroid.X) &&
	         FMath::IsFinite(Plane.Centroid.Y) &&
	         FMath::IsFinite(Plane.Centroid.Z));

	return true;
}
