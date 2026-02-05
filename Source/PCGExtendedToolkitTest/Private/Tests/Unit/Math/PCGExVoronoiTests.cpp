// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExVoronoi Unit Tests
 *
 * Tests Voronoi diagram utilities:
 * - TVoronoi2 (2D Voronoi from Delaunay triangulation)
 * - TVoronoi3 (3D Voronoi from Delaunay tetrahedralization)
 * - Different metrics (Euclidean, Manhattan, Chebyshev)
 * - Cell center methods (Circumcenter, Centroid, Balanced)
 *
 * These are pure geometry tests - no game world required.
 *
 * Test naming convention: PCGEx.Unit.Voronoi.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/Geo/PCGExVoronoi.h"
#include "Math/Geo/PCGExDelaunay.h"
#include "Math/PCGExProjectionDetails.h"
#include "PCGExH.h"

using namespace PCGExMath::Geo;

// =============================================================================
// TVoronoi2 Basic Tests
// =============================================================================

/**
 * Test TVoronoi2::Process with triangle (3 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessTriangleTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Triangle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessTriangleTest::RunTest(const FString& Parameters)
{
	// Create 3 points forming a triangle
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Voronoi2 processes triangle successfully"), bResult);
	TestTrue(TEXT("Voronoi2 is valid"), Voronoi.IsValid);
	TestTrue(TEXT("Has Delaunay"), Voronoi.Delaunay.IsValid());

	// One triangle = one Delaunay site = one circumcenter/centroid
	TestEqual(TEXT("One circumcenter"), Voronoi.Circumcenters.Num(), 1);
	TestEqual(TEXT("One centroid"), Voronoi.Centroids.Num(), 1);

	// Single site has no adjacent sites, so no Voronoi edges
	TestEqual(TEXT("No Voronoi edges for single triangle"), Voronoi.VoronoiEdges.Num(), 0);

	return true;
}

/**
 * Test TVoronoi2::Process with square (4 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessSquareTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Square",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessSquareTest::RunTest(const FString& Parameters)
{
	// Create 4 points forming a square
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Voronoi2 processes square successfully"), bResult);
	TestTrue(TEXT("Voronoi2 is valid"), Voronoi.IsValid);

	// Square triangulates to 2 triangles
	TestEqual(TEXT("Two circumcenters"), Voronoi.Circumcenters.Num(), 2);
	TestEqual(TEXT("Two centroids"), Voronoi.Centroids.Num(), 2);

	// Two adjacent triangles = one Voronoi edge between them
	TestEqual(TEXT("One Voronoi edge"), Voronoi.VoronoiEdges.Num(), 1);

	return true;
}

/**
 * Test TVoronoi2::Process with grid (9 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessGridTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessGridTest::RunTest(const FString& Parameters)
{
	// Create 9 points (3x3 grid)
	TArray<FVector> Positions;
	for (int32 y = 0; y < 3; y++)
	{
		for (int32 x = 0; x < 3; x++)
		{
			Positions.Add(FVector(x * 100.0f, y * 100.0f, 0.0f));
		}
	}

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Voronoi2 processes grid successfully"), bResult);
	TestTrue(TEXT("Voronoi2 is valid"), Voronoi.IsValid);
	TestTrue(TEXT("Multiple circumcenters"), Voronoi.Circumcenters.Num() > 2);
	TestTrue(TEXT("Multiple Voronoi edges"), Voronoi.VoronoiEdges.Num() > 0);

	return true;
}

/**
 * Test TVoronoi2::Process edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessEdgeCasesTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessEdgeCasesTest::RunTest(const FString& Parameters)
{
	FPCGExGeo2DProjectionDetails Projection;
	TVoronoi2 Voronoi;

	// Empty array
	TArray<FVector> Empty;
	TestFalse(TEXT("Empty array returns false"), Voronoi.Process(MakeArrayView(Empty), Projection));
	TestFalse(TEXT("IsValid is false after empty"), Voronoi.IsValid);

	// Too few points
	TArray<FVector> TooFew = {
		FVector(0, 0, 0),
		FVector(100, 0, 0)
	};
	TestFalse(TEXT("2 points returns false"), Voronoi.Process(MakeArrayView(TooFew), Projection));

	return true;
}

// =============================================================================
// TVoronoi2 Bounds Tests
// =============================================================================

/**
 * Test TVoronoi2::Process with bounds checking
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessWithBoundsTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.WithBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessWithBoundsTest::RunTest(const FString& Parameters)
{
	// Create points that will have circumcenters both inside and outside bounds
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0),
		FVector(50, 50, 0) // Center point
	};

	FPCGExGeo2DProjectionDetails Projection;
	FBox Bounds(FVector(0, 0, -10), FVector(100, 100, 10));
	TBitArray<> WithinBounds;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection, Bounds, WithinBounds);

	TestTrue(TEXT("Voronoi2 with bounds processes successfully"), bResult);
	TestTrue(TEXT("WithinBounds populated"), WithinBounds.Num() > 0);
	TestEqual(TEXT("WithinBounds size matches sites"), WithinBounds.Num(), Voronoi.Circumcenters.Num());

	return true;
}

// =============================================================================
// TVoronoi2 Metric Tests
// =============================================================================

/**
 * Test TVoronoi2::Process with Euclidean metric
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessEuclideanTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Metric.Euclidean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessEuclideanTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection,
	                               EPCGExVoronoiMetric::Euclidean, EPCGExCellCenter::Circumcenter);

	TestTrue(TEXT("Euclidean metric processes successfully"), bResult);
	TestEqual(TEXT("Metric is Euclidean"), Voronoi.Metric, EPCGExVoronoiMetric::Euclidean);
	TestTrue(TEXT("OutputVertices populated"), Voronoi.OutputVertices.Num() > 0);
	TestTrue(TEXT("OutputEdges populated"), Voronoi.OutputEdges.Num() > 0);
	TestTrue(TEXT("NumCellCenters set"), Voronoi.NumCellCenters > 0);

	return true;
}

/**
 * Test TVoronoi2::Process with Manhattan metric
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessManhattanTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Metric.Manhattan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessManhattanTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection,
	                               EPCGExVoronoiMetric::Manhattan, EPCGExCellCenter::Circumcenter);

	TestTrue(TEXT("Manhattan metric processes successfully"), bResult);
	TestEqual(TEXT("Metric is Manhattan"), Voronoi.Metric, EPCGExVoronoiMetric::Manhattan);
	TestTrue(TEXT("OutputVertices populated"), Voronoi.OutputVertices.Num() > 0);

	return true;
}

/**
 * Test TVoronoi2::Process with Chebyshev metric
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessChebyshevTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.Metric.Chebyshev",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessChebyshevTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection,
	                               EPCGExVoronoiMetric::Chebyshev, EPCGExCellCenter::Circumcenter);

	TestTrue(TEXT("Chebyshev metric processes successfully"), bResult);
	TestEqual(TEXT("Metric is Chebyshev"), Voronoi.Metric, EPCGExVoronoiMetric::Chebyshev);
	TestTrue(TEXT("OutputVertices populated"), Voronoi.OutputVertices.Num() > 0);

	return true;
}

// =============================================================================
// TVoronoi2 Cell Center Method Tests
// =============================================================================

/**
 * Test TVoronoi2::Process with Centroid cell center method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessCentroidMethodTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.CellCenter.Centroid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessCentroidMethodTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection,
	                               EPCGExVoronoiMetric::Euclidean, EPCGExCellCenter::Centroid);

	TestTrue(TEXT("Centroid method processes successfully"), bResult);

	// With centroid method, cell center should be average of triangle vertices
	// For triangle (0,0), (100,0), (50,100): centroid = (50, 33.33, 0)
	FVector ExpectedCentroid(50.0f, 100.0f / 3.0f, 0.0f);
	TestTrue(TEXT("Cell center near expected centroid"),
	         Voronoi.OutputVertices[0].Equals(ExpectedCentroid, 1.0f));

	return true;
}

/**
 * Test TVoronoi2::Process with Balanced cell center method
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi2ProcessBalancedMethodTest,
	"PCGEx.Unit.Voronoi.Voronoi2.Process.CellCenter.Balanced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi2ProcessBalancedMethodTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;
	FBox Bounds(FVector(-10, -10, -10), FVector(110, 110, 10));
	TBitArray<> WithinBounds;

	TVoronoi2 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions), Projection, Bounds, WithinBounds,
	                               EPCGExVoronoiMetric::Euclidean, EPCGExCellCenter::Balanced);

	TestTrue(TEXT("Balanced method processes successfully"), bResult);
	TestTrue(TEXT("OutputVertices populated"), Voronoi.OutputVertices.Num() > 0);

	return true;
}

// =============================================================================
// TVoronoi3 Tests
// =============================================================================

/**
 * Test TVoronoi3::Process with tetrahedron (4 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi3ProcessTetrahedronTest,
	"PCGEx.Unit.Voronoi.Voronoi3.Process.Tetrahedron",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi3ProcessTetrahedronTest::RunTest(const FString& Parameters)
{
	// Create 4 points forming a tetrahedron
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0),
		FVector(50, 50, 100)
	};

	TVoronoi3 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions));

	TestTrue(TEXT("Voronoi3 processes tetrahedron successfully"), bResult);
	TestTrue(TEXT("Voronoi3 is valid"), Voronoi.IsValid);
	TestTrue(TEXT("Has Delaunay"), Voronoi.Delaunay.IsValid());

	// One tetrahedron = one Delaunay site
	TestEqual(TEXT("One circumsphere"), Voronoi.Circumspheres.Num(), 1);
	TestEqual(TEXT("One centroid"), Voronoi.Centroids.Num(), 1);

	// Single site has no adjacent sites, so no Voronoi edges
	TestEqual(TEXT("No Voronoi edges for single tetrahedron"), Voronoi.VoronoiEdges.Num(), 0);

	return true;
}

/**
 * Test TVoronoi3::Process with cube (8 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi3ProcessCubeTest,
	"PCGEx.Unit.Voronoi.Voronoi3.Process.Cube",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi3ProcessCubeTest::RunTest(const FString& Parameters)
{
	// Create 8 points (cube corners)
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0),
		FVector(0, 0, 100),
		FVector(100, 0, 100),
		FVector(100, 100, 100),
		FVector(0, 100, 100)
	};

	TVoronoi3 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions));

	TestTrue(TEXT("Voronoi3 processes cube successfully"), bResult);
	TestTrue(TEXT("Voronoi3 is valid"), Voronoi.IsValid);
	TestTrue(TEXT("Multiple circumspheres"), Voronoi.Circumspheres.Num() > 1);
	TestTrue(TEXT("Multiple centroids"), Voronoi.Centroids.Num() > 1);
	TestTrue(TEXT("Has Voronoi edges"), Voronoi.VoronoiEdges.Num() > 0);

	return true;
}

/**
 * Test TVoronoi3::Process with 5 points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi3ProcessFivePointsTest,
	"PCGEx.Unit.Voronoi.Voronoi3.Process.FivePoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi3ProcessFivePointsTest::RunTest(const FString& Parameters)
{
	// Create 5 points - tetrahedron with center point
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0),
		FVector(50, 50, 100),
		FVector(50, 40, 30) // Point inside
	};

	TVoronoi3 Voronoi;
	bool bResult = Voronoi.Process(MakeArrayView(Positions));

	TestTrue(TEXT("Voronoi3 with 5 points processes successfully"), bResult);
	TestTrue(TEXT("Multiple tetrahedra"), Voronoi.Delaunay->Sites.Num() > 1);
	TestTrue(TEXT("Has Voronoi edges"), Voronoi.VoronoiEdges.Num() > 0);

	// Verify circumspheres are valid
	for (const FSphere& Sphere : Voronoi.Circumspheres)
	{
		TestTrue(TEXT("Circumsphere has positive radius"), Sphere.W > 0.0f);
	}

	return true;
}

/**
 * Test TVoronoi3::Process edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoi3ProcessEdgeCasesTest,
	"PCGEx.Unit.Voronoi.Voronoi3.Process.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoi3ProcessEdgeCasesTest::RunTest(const FString& Parameters)
{
	TVoronoi3 Voronoi;

	// Empty array
	TArray<FVector> Empty;
	TestFalse(TEXT("Empty array returns false"), Voronoi.Process(MakeArrayView(Empty)));
	TestFalse(TEXT("IsValid is false after empty"), Voronoi.IsValid);

	// Too few points (need at least 4 for 3D)
	TArray<FVector> TooFew = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};
	TestFalse(TEXT("3 points returns false"), Voronoi.Process(MakeArrayView(TooFew)));

	return true;
}

// =============================================================================
// Enum Tests
// =============================================================================

/**
 * Test EPCGExVoronoiMetric enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExVoronoiMetricEnumTest,
	"PCGEx.Unit.Voronoi.Enum.VoronoiMetric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExVoronoiMetricEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Euclidean = 0"),
	          static_cast<uint8>(EPCGExVoronoiMetric::Euclidean), static_cast<uint8>(0));
	TestEqual(TEXT("Manhattan = 1"),
	          static_cast<uint8>(EPCGExVoronoiMetric::Manhattan), static_cast<uint8>(1));
	TestEqual(TEXT("Chebyshev = 2"),
	          static_cast<uint8>(EPCGExVoronoiMetric::Chebyshev), static_cast<uint8>(2));

	return true;
}

/**
 * Test EPCGExCellCenter enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCellCenterEnumTest,
	"PCGEx.Unit.Voronoi.Enum.CellCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCellCenterEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Balanced = 0"),
	          static_cast<uint8>(EPCGExCellCenter::Balanced), static_cast<uint8>(0));
	TestEqual(TEXT("Circumcenter = 1"),
	          static_cast<uint8>(EPCGExCellCenter::Circumcenter), static_cast<uint8>(1));
	TestEqual(TEXT("Centroid = 2"),
	          static_cast<uint8>(EPCGExCellCenter::Centroid), static_cast<uint8>(2));

	return true;
}
