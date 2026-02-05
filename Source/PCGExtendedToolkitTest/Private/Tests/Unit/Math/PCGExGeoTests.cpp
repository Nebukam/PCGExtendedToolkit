// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExGeo Unit Tests
 *
 * Tests geometry functions in PCGExGeo.h:
 * - Det: 2D determinant
 * - GetCircumcenter / GetCircumcenter2D: Circumcenter of triangles
 * - GetCentroid: Centroid of triangles/tetrahedra
 * - GetBarycentricCoordinates: Barycentric coordinates
 * - IsPointInTriangle: Point-in-triangle test
 * - IsPointInPolygon: 2D point-in-polygon test
 * - TransformToLInf / TransformFromLInf: L-infinity coordinate transforms
 * - ComputeLInfEdgePath / ComputeL1EdgePath: Voronoi edge path computation
 *
 * Test naming convention: PCGEx.Unit.Math.Geo.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/Geo/PCGExGeo.h"
#include "Helpers/PCGExTestHelpers.h"

// =============================================================================
// Det (2D Determinant) Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoDetBasicTest,
	"PCGEx.Unit.Math.Geo.Det.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoDetBasicTest::RunTest(const FString& Parameters)
{
	// Det(A, B) = A.X * B.Y - A.Y * B.X (cross product Z component)

	// Unit vectors
	FVector2D A(1, 0);
	FVector2D B(0, 1);
	TestTrue(TEXT("Det of perpendicular unit vectors = 1"),
		FMath::IsNearlyEqual(PCGExMath::Geo::Det(A, B), 1.0, KINDA_SMALL_NUMBER));

	// Reversed order
	TestTrue(TEXT("Det of reversed perpendicular vectors = -1"),
		FMath::IsNearlyEqual(PCGExMath::Geo::Det(B, A), -1.0, KINDA_SMALL_NUMBER));

	// Parallel vectors
	FVector2D C(2, 0);
	TestTrue(TEXT("Det of parallel vectors = 0"),
		FMath::IsNearlyZero(PCGExMath::Geo::Det(A, C), KINDA_SMALL_NUMBER));

	// Arbitrary vectors
	FVector2D D(3, 4);
	FVector2D E(2, 5);
	// Det = 3*5 - 4*2 = 15 - 8 = 7
	TestTrue(TEXT("Det of arbitrary vectors"),
		FMath::IsNearlyEqual(PCGExMath::Geo::Det(D, E), 7.0, KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoDetFVectorTest,
	"PCGEx.Unit.Math.Geo.Det.FVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoDetFVectorTest::RunTest(const FString& Parameters)
{
	// Det also works with FVector (uses only X, Y)
	FVector A(1, 0, 100);  // Z ignored
	FVector B(0, 1, 200);  // Z ignored

	TestTrue(TEXT("Det with FVector ignores Z"),
		FMath::IsNearlyEqual(PCGExMath::Geo::Det(A, B), 1.0, KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// GetCentroid Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetCentroidTriangleTest,
	"PCGEx.Unit.Math.Geo.GetCentroid.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetCentroidTriangleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Simple triangle at origin
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(3, 0, 0),
		FVector(0, 3, 0)
	};

	int32 Vtx[3] = {0, 1, 2};
	FVector Centroid;
	PCGExMath::Geo::GetCentroid(TArrayView<FVector>(Positions), Vtx, Centroid);

	// Centroid is average of vertices
	FVector Expected(1.0, 1.0, 0.0);
	TestTrue(TEXT("Triangle centroid"),
		PCGExTest::NearlyEqual(Centroid, Expected, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetCentroidTetrahedronTest,
	"PCGEx.Unit.Math.Geo.GetCentroid.Tetrahedron",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetCentroidTetrahedronTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Regular tetrahedron
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(4, 0, 0),
		FVector(2, 4, 0),
		FVector(2, 2, 4)
	};

	int32 Vtx[4] = {0, 1, 2, 3};
	FVector Centroid;
	PCGExMath::Geo::GetCentroid(TArrayView<FVector>(Positions), Vtx, Centroid);

	// Centroid is average of vertices
	FVector Expected(2.0, 1.5, 1.0);
	TestTrue(TEXT("Tetrahedron centroid"),
		PCGExTest::NearlyEqual(Centroid, Expected, Tolerance));

	return true;
}

// =============================================================================
// GetCircumcenter Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetCircumcenterRightTriangleTest,
	"PCGEx.Unit.Math.Geo.GetCircumcenter.RightTriangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetCircumcenterRightTriangleTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Right triangle: circumcenter is at midpoint of hypotenuse
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(4, 0, 0),
		FVector(0, 4, 0)
	};

	int32 Vtx[3] = {0, 1, 2};
	FVector Circumcenter;
	PCGExMath::Geo::GetCircumcenter(TArrayView<FVector>(Positions), Vtx, Circumcenter);

	// For right triangle, circumcenter is midpoint of hypotenuse
	FVector Expected(2.0, 2.0, 0.0);
	TestTrue(TEXT("Right triangle circumcenter at hypotenuse midpoint"),
		PCGExTest::NearlyEqual(Circumcenter, Expected, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetCircumcenterEquilateralTest,
	"PCGEx.Unit.Math.Geo.GetCircumcenter.Equilateral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetCircumcenterEquilateralTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// Equilateral triangle: circumcenter equals centroid
	const double H = FMath::Sqrt(3.0) / 2.0;  // Height of equilateral triangle
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(0.5, H, 0)
	};

	int32 Vtx[3] = {0, 1, 2};
	FVector Circumcenter;
	PCGExMath::Geo::GetCircumcenter(TArrayView<FVector>(Positions), Vtx, Circumcenter);

	FVector Centroid;
	PCGExMath::Geo::GetCentroid(TArrayView<FVector>(Positions), Vtx, Centroid);

	// For equilateral triangle, circumcenter == centroid
	TestTrue(TEXT("Equilateral circumcenter equals centroid"),
		PCGExTest::NearlyEqual(Circumcenter, Centroid, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetCircumcenter2DTest,
	"PCGEx.Unit.Math.Geo.GetCircumcenter2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetCircumcenter2DTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Triangle with non-zero Z values
	TArray<FVector> Positions = {
		FVector(0, 0, 10),
		FVector(4, 0, 20),
		FVector(0, 4, 30)
	};

	int32 Vtx[3] = {0, 1, 2};
	FVector Circumcenter;
	PCGExMath::Geo::GetCircumcenter2D(TArrayView<FVector>(Positions), Vtx, Circumcenter);

	// 2D circumcenter: X,Y from 2D computation, Z averaged
	TestTrue(TEXT("2D circumcenter X"),
		FMath::IsNearlyEqual(Circumcenter.X, 2.0, Tolerance));
	TestTrue(TEXT("2D circumcenter Y"),
		FMath::IsNearlyEqual(Circumcenter.Y, 2.0, Tolerance));
	TestTrue(TEXT("2D circumcenter Z is average"),
		FMath::IsNearlyEqual(Circumcenter.Z, 20.0, Tolerance));

	return true;
}

// =============================================================================
// GetBarycentricCoordinates Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetBarycentricVerticesTest,
	"PCGEx.Unit.Math.Geo.GetBarycentricCoordinates.Vertices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetBarycentricVerticesTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	FVector A(0, 0, 0);
	FVector B(4, 0, 0);
	FVector C(0, 4, 0);

	// At vertex A: barycentric = (1, 0, 0)
	FVector BaryA = PCGExMath::Geo::GetBarycentricCoordinates(A, A, B, C);
	TestTrue(TEXT("Barycentric at A"),
		FMath::IsNearlyEqual(BaryA.X, 1.0, Tolerance) &&
		FMath::IsNearlyZero(BaryA.Y, Tolerance) &&
		FMath::IsNearlyZero(BaryA.Z, Tolerance));

	// At vertex B: barycentric = (0, 1, 0)
	FVector BaryB = PCGExMath::Geo::GetBarycentricCoordinates(B, A, B, C);
	TestTrue(TEXT("Barycentric at B"),
		FMath::IsNearlyZero(BaryB.X, Tolerance) &&
		FMath::IsNearlyEqual(BaryB.Y, 1.0, Tolerance) &&
		FMath::IsNearlyZero(BaryB.Z, Tolerance));

	// At vertex C: barycentric = (0, 0, 1)
	FVector BaryC = PCGExMath::Geo::GetBarycentricCoordinates(C, A, B, C);
	TestTrue(TEXT("Barycentric at C"),
		FMath::IsNearlyZero(BaryC.X, Tolerance) &&
		FMath::IsNearlyZero(BaryC.Y, Tolerance) &&
		FMath::IsNearlyEqual(BaryC.Z, 1.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetBarycentricCentroidTest,
	"PCGEx.Unit.Math.Geo.GetBarycentricCoordinates.Centroid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetBarycentricCentroidTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	FVector A(0, 0, 0);
	FVector B(3, 0, 0);
	FVector C(0, 3, 0);
	FVector Centroid(1, 1, 0);  // Centroid of triangle

	FVector Bary = PCGExMath::Geo::GetBarycentricCoordinates(Centroid, A, B, C);

	// At centroid, all barycentric coords should be equal (1/3 each)
	TestTrue(TEXT("Barycentric at centroid: all coords equal"),
		FMath::IsNearlyEqual(Bary.X, Bary.Y, Tolerance) &&
		FMath::IsNearlyEqual(Bary.Y, Bary.Z, Tolerance));

	// Sum should be 1
	TestTrue(TEXT("Barycentric sum = 1"),
		FMath::IsNearlyEqual(Bary.X + Bary.Y + Bary.Z, 1.0, Tolerance));

	return true;
}

// =============================================================================
// IsPointInTriangle Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInTriangleInsideTest,
	"PCGEx.Unit.Math.Geo.IsPointInTriangle.Inside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInTriangleInsideTest::RunTest(const FString& Parameters)
{
	FVector A(0, 0, 0);
	FVector B(4, 0, 0);
	FVector C(0, 4, 0);

	// Centroid is definitely inside
	FVector Centroid(4.0/3.0, 4.0/3.0, 0);
	TestTrue(TEXT("Centroid is inside triangle"),
		PCGExMath::Geo::IsPointInTriangle(Centroid, A, B, C));

	// Another interior point
	FVector Interior(1, 1, 0);
	TestTrue(TEXT("Interior point is inside triangle"),
		PCGExMath::Geo::IsPointInTriangle(Interior, A, B, C));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInTriangleOutsideTest,
	"PCGEx.Unit.Math.Geo.IsPointInTriangle.Outside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInTriangleOutsideTest::RunTest(const FString& Parameters)
{
	FVector A(0, 0, 0);
	FVector B(4, 0, 0);
	FVector C(0, 4, 0);

	// Clearly outside
	FVector Outside1(-1, -1, 0);
	TestFalse(TEXT("Point at (-1,-1) is outside"),
		PCGExMath::Geo::IsPointInTriangle(Outside1, A, B, C));

	FVector Outside2(5, 5, 0);
	TestFalse(TEXT("Point at (5,5) is outside"),
		PCGExMath::Geo::IsPointInTriangle(Outside2, A, B, C));

	FVector Outside3(3, 3, 0);  // Beyond hypotenuse
	TestFalse(TEXT("Point beyond hypotenuse is outside"),
		PCGExMath::Geo::IsPointInTriangle(Outside3, A, B, C));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInTriangleOnEdgeTest,
	"PCGEx.Unit.Math.Geo.IsPointInTriangle.OnEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInTriangleOnEdgeTest::RunTest(const FString& Parameters)
{
	FVector A(0, 0, 0);
	FVector B(4, 0, 0);
	FVector C(0, 4, 0);

	// On edge AB
	FVector OnAB(2, 0, 0);
	TestTrue(TEXT("Point on edge AB is inside (boundary)"),
		PCGExMath::Geo::IsPointInTriangle(OnAB, A, B, C));

	// At vertex
	TestTrue(TEXT("Vertex A is inside (boundary)"),
		PCGExMath::Geo::IsPointInTriangle(A, A, B, C));

	return true;
}

// =============================================================================
// IsPointInPolygon Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInPolygonSquareTest,
	"PCGEx.Unit.Math.Geo.IsPointInPolygon.Square",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInPolygonSquareTest::RunTest(const FString& Parameters)
{
	// Unit square
	TArray<FVector2D> Square = {
		FVector2D(0, 0),
		FVector2D(1, 0),
		FVector2D(1, 1),
		FVector2D(0, 1)
	};

	// Inside
	TestTrue(TEXT("Center is inside square"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(0.5, 0.5), Square));

	// Outside
	TestFalse(TEXT("Point outside square"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(2, 2), Square));

	TestFalse(TEXT("Point to left is outside"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(-0.5, 0.5), Square));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInPolygonConcaveTest,
	"PCGEx.Unit.Math.Geo.IsPointInPolygon.Concave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInPolygonConcaveTest::RunTest(const FString& Parameters)
{
	// L-shaped polygon (concave)
	TArray<FVector2D> LShape = {
		FVector2D(0, 0),
		FVector2D(2, 0),
		FVector2D(2, 1),
		FVector2D(1, 1),
		FVector2D(1, 2),
		FVector2D(0, 2)
	};

	// In the bottom part
	TestTrue(TEXT("Point in bottom part of L"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(1.5, 0.5), LShape));

	// In the left part
	TestTrue(TEXT("Point in left part of L"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(0.5, 1.5), LShape));

	// In the concave region (outside)
	TestFalse(TEXT("Point in concave region is outside"),
		PCGExMath::Geo::IsPointInPolygon(FVector2D(1.5, 1.5), LShape));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoIsPointInPolygonFVector3DTest,
	"PCGEx.Unit.Math.Geo.IsPointInPolygon.FVector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoIsPointInPolygonFVector3DTest::RunTest(const FString& Parameters)
{
	// Test the FVector overload (uses only X, Y)
	TArray<FVector2D> Square = {
		FVector2D(0, 0),
		FVector2D(1, 0),
		FVector2D(1, 1),
		FVector2D(0, 1)
	};

	FVector Point3D(0.5, 0.5, 100.0);  // Z is ignored
	TestTrue(TEXT("FVector inside square (Z ignored)"),
		PCGExMath::Geo::IsPointInPolygon(Point3D, Square));

	return true;
}

// =============================================================================
// L-Infinity Transform Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoTransformLInfTest,
	"PCGEx.Unit.Math.Geo.TransformLInf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoTransformLInfTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Transform: (x,y) -> (x+y, x-y)
	FVector2D P(3, 2);
	FVector2D Transformed = PCGExMath::Geo::TransformToLInf(P);

	TestTrue(TEXT("TransformToLInf X = x+y"),
		FMath::IsNearlyEqual(Transformed.X, 5.0, Tolerance));
	TestTrue(TEXT("TransformToLInf Y = x-y"),
		FMath::IsNearlyEqual(Transformed.Y, 1.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoTransformFromLInfTest,
	"PCGEx.Unit.Math.Geo.TransformFromLInf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoTransformFromLInfTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Inverse transform: (u,v) -> ((u+v)/2, (u-v)/2)
	FVector2D P(5, 1);  // u=5, v=1
	FVector2D Inverse = PCGExMath::Geo::TransformFromLInf(P);

	// Expected: ((5+1)/2, (5-1)/2) = (3, 2)
	TestTrue(TEXT("TransformFromLInf X = (u+v)/2"),
		FMath::IsNearlyEqual(Inverse.X, 3.0, Tolerance));
	TestTrue(TEXT("TransformFromLInf Y = (u-v)/2"),
		FMath::IsNearlyEqual(Inverse.Y, 2.0, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoTransformLInfRoundTripTest,
	"PCGEx.Unit.Math.Geo.TransformLInf.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoTransformLInfRoundTripTest::RunTest(const FString& Parameters)
{
	const double Tolerance = KINDA_SMALL_NUMBER;

	// Round trip should return original
	FVector2D Original(7, 3);
	FVector2D RoundTrip = PCGExMath::Geo::TransformFromLInf(
		PCGExMath::Geo::TransformToLInf(Original));

	TestTrue(TEXT("Round trip preserves point"),
		FMath::IsNearlyEqual(RoundTrip.X, Original.X, Tolerance) &&
		FMath::IsNearlyEqual(RoundTrip.Y, Original.Y, Tolerance));

	return true;
}

// =============================================================================
// ComputeLInfEdgePath Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoComputeLInfEdgePathHorizontalTest,
	"PCGEx.Unit.Math.Geo.ComputeLInfEdgePath.Horizontal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoComputeLInfEdgePathHorizontalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Horizontal line - no bend needed
	FVector2D Start(0, 0);
	FVector2D End(4, 0);
	TArray<FVector2D> Path;

	PCGExMath::Geo::ComputeLInfEdgePath(Start, End, Path);

	TestTrue(TEXT("Horizontal path has at least 2 points"),
		Path.Num() >= 2);
	TestTrue(TEXT("Path starts at Start"),
		Path[0].Equals(Start, Tolerance));
	TestTrue(TEXT("Path ends at End"),
		Path.Last().Equals(End, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoComputeLInfEdgePathDiagonalTest,
	"PCGEx.Unit.Math.Geo.ComputeLInfEdgePath.Diagonal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoComputeLInfEdgePathDiagonalTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// 45-degree diagonal - no bend needed
	FVector2D Start(0, 0);
	FVector2D End(3, 3);
	TArray<FVector2D> Path;

	PCGExMath::Geo::ComputeLInfEdgePath(Start, End, Path);

	TestTrue(TEXT("Diagonal path has at least 2 points"),
		Path.Num() >= 2);
	TestTrue(TEXT("Path starts at Start"),
		Path[0].Equals(Start, Tolerance));
	TestTrue(TEXT("Path ends at End"),
		Path.Last().Equals(End, Tolerance));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoComputeLInfEdgePathBendTest,
	"PCGEx.Unit.Math.Geo.ComputeLInfEdgePath.WithBend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoComputeLInfEdgePathBendTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	// Non-axis-aligned, non-45-degree - should have bend
	FVector2D Start(0, 0);
	FVector2D End(4, 2);  // Neither horizontal nor 45-degree
	TArray<FVector2D> Path;

	PCGExMath::Geo::ComputeLInfEdgePath(Start, End, Path);

	TestTrue(TEXT("Path with bend has at least 2 points"),
		Path.Num() >= 2);
	TestTrue(TEXT("Path starts at Start"),
		Path[0].Equals(Start, Tolerance));
	TestTrue(TEXT("Path ends at End"),
		Path.Last().Equals(End, Tolerance));

	return true;
}

// =============================================================================
// ComputeL1EdgePath Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoComputeL1EdgePathBasicTest,
	"PCGEx.Unit.Math.Geo.ComputeL1EdgePath.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoComputeL1EdgePathBasicTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.01;

	FVector2D Start(0, 0);
	FVector2D End(3, 2);
	TArray<FVector2D> Path;

	PCGExMath::Geo::ComputeL1EdgePath(Start, End, Path);

	TestTrue(TEXT("L1 path has at least 2 points"),
		Path.Num() >= 2);
	TestTrue(TEXT("L1 path starts at Start"),
		Path[0].Equals(Start, Tolerance));
	TestTrue(TEXT("L1 path ends at End"),
		Path.Last().Equals(End, Tolerance));

	return true;
}

// =============================================================================
// FindSphereFrom4Points Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoFindSphereFrom4PointsRegularTest,
	"PCGEx.Unit.Math.Geo.FindSphereFrom4Points.Regular",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoFindSphereFrom4PointsRegularTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// 4 points on a unit sphere centered at origin
	TArray<FVector> Positions = {
		FVector(1, 0, 0),
		FVector(-1, 0, 0),
		FVector(0, 1, 0),
		FVector(0, 0, 1)
	};

	int32 Vtx[4] = {0, 1, 2, 3};
	FSphere Sphere;
	bool bSuccess = PCGExMath::Geo::FindSphereFrom4Points(TArrayView<FVector>(Positions), Vtx, Sphere);

	TestTrue(TEXT("Sphere found successfully"), bSuccess);

	if (bSuccess)
	{
		// Center should be at origin
		TestTrue(TEXT("Sphere center near origin"),
			PCGExTest::NearlyEqual(FVector(Sphere.Center), FVector::ZeroVector, Tolerance));

		// Radius should be 1
		TestTrue(TEXT("Sphere radius is 1"),
			FMath::IsNearlyEqual(Sphere.W, 1.0, Tolerance));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoFindSphereFrom4PointsOffsetTest,
	"PCGEx.Unit.Math.Geo.FindSphereFrom4Points.Offset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoFindSphereFrom4PointsOffsetTest::RunTest(const FString& Parameters)
{
	const double Tolerance = 0.1;

	// 4 points on a sphere centered at (10, 10, 10) with radius 2
	FVector Center(10, 10, 10);
	double Radius = 2.0;

	TArray<FVector> Positions = {
		Center + FVector(Radius, 0, 0),
		Center + FVector(-Radius, 0, 0),
		Center + FVector(0, Radius, 0),
		Center + FVector(0, 0, Radius)
	};

	int32 Vtx[4] = {0, 1, 2, 3};
	FSphere Sphere;
	bool bSuccess = PCGExMath::Geo::FindSphereFrom4Points(TArrayView<FVector>(Positions), Vtx, Sphere);

	TestTrue(TEXT("Offset sphere found"), bSuccess);

	if (bSuccess)
	{
		TestTrue(TEXT("Offset sphere center"),
			PCGExTest::NearlyEqual(FVector(Sphere.Center), Center, Tolerance));
		TestTrue(TEXT("Offset sphere radius"),
			FMath::IsNearlyEqual(Sphere.W, Radius, Tolerance));
	}

	return true;
}

// =============================================================================
// GetLongestEdge Tests
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetLongestEdgeTriangleTest,
	"PCGEx.Unit.Math.Geo.GetLongestEdge.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetLongestEdgeTriangleTest::RunTest(const FString& Parameters)
{
	// Right triangle: hypotenuse should be longest
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(3, 0, 0),
		FVector(0, 4, 0)
	};

	int32 Vtx[3] = {0, 1, 2};
	uint64 Edge = 0;
	PCGExMath::Geo::GetLongestEdge(TArrayView<FVector>(Positions), Vtx, Edge);

	// Edge should be non-zero (valid edge found)
	TestTrue(TEXT("Longest edge found"), Edge != 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGeoGetLongestEdgeTetrahedronTest,
	"PCGEx.Unit.Math.Geo.GetLongestEdge.Tetrahedron",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGeoGetLongestEdgeTetrahedronTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(1, 0, 0),
		FVector(0.5, 1, 0),
		FVector(0.5, 0.5, 10)  // Very tall - edges to this point are longest
	};

	int32 Vtx[4] = {0, 1, 2, 3};
	uint64 Edge = 0;
	PCGExMath::Geo::GetLongestEdge(TArrayView<FVector>(Positions), Vtx, Edge);

	TestTrue(TEXT("Longest edge in tetrahedron found"), Edge != 0);

	return true;
}
