// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExDelaunay Unit Tests
 *
 * Tests Delaunay triangulation utilities:
 * - FDelaunaySite2 (2D triangles)
 * - FDelaunaySite3 (3D tetrahedra)
 * - TDelaunay2 (2D triangulation)
 * - TDelaunay3 (3D triangulation)
 *
 * These are pure geometry tests - no game world required.
 *
 * Test naming convention: PCGEx.Unit.Delaunay.<Category>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Math/Geo/PCGExDelaunay.h"
#include "Math/PCGExProjectionDetails.h"
#include "PCGExH.h"

using namespace PCGExMath::Geo;

// =============================================================================
// FDelaunaySite2 Tests
// =============================================================================

/**
 * Test FDelaunaySite2 construction from vertex indices
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite2ConstructorTest,
	"PCGEx.Unit.Delaunay.Site2.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite2ConstructorTest::RunTest(const FString& Parameters)
{
	// Test 3-argument constructor
	FDelaunaySite2 Site(0, 1, 2, 42);

	TestEqual(TEXT("Vtx[0] is A"), Site.Vtx[0], 0);
	TestEqual(TEXT("Vtx[1] is B"), Site.Vtx[1], 1);
	TestEqual(TEXT("Vtx[2] is C"), Site.Vtx[2], 2);
	TestEqual(TEXT("Id is set"), Site.Id, 42);

	// Neighbors should be -1 initially
	TestEqual(TEXT("Neighbor[0] is -1"), Site.Neighbors[0], -1);
	TestEqual(TEXT("Neighbor[1] is -1"), Site.Neighbors[1], -1);
	TestEqual(TEXT("Neighbor[2] is -1"), Site.Neighbors[2], -1);

	return true;
}

/**
 * Test FDelaunaySite2 edge hashing functions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite2EdgeHashTest,
	"PCGEx.Unit.Delaunay.Site2.EdgeHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite2EdgeHashTest::RunTest(const FString& Parameters)
{
	FDelaunaySite2 Site(10, 20, 30, 0);

	// Test edge hash functions
	uint64 AB = Site.AB();
	uint64 BC = Site.BC();
	uint64 AC = Site.AC();

	// Verify these are unique (different edges)
	TestNotEqual(TEXT("AB != BC"), AB, BC);
	TestNotEqual(TEXT("AB != AC"), AB, AC);
	TestNotEqual(TEXT("BC != AC"), BC, AC);

	// Verify they match the H64U hash function
	TestEqual(TEXT("AB matches H64U(10,20)"), AB, PCGEx::H64U(10, 20));
	TestEqual(TEXT("BC matches H64U(20,30)"), BC, PCGEx::H64U(20, 30));
	TestEqual(TEXT("AC matches H64U(10,30)"), AC, PCGEx::H64U(10, 30));

	return true;
}

/**
 * Test FDelaunaySite2::ContainsEdge
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite2ContainsEdgeTest,
	"PCGEx.Unit.Delaunay.Site2.ContainsEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite2ContainsEdgeTest::RunTest(const FString& Parameters)
{
	FDelaunaySite2 Site(5, 10, 15, 0);

	// Test edges that exist
	TestTrue(TEXT("Contains AB edge"), Site.ContainsEdge(PCGEx::H64U(5, 10)));
	TestTrue(TEXT("Contains BC edge"), Site.ContainsEdge(PCGEx::H64U(10, 15)));
	TestTrue(TEXT("Contains AC edge"), Site.ContainsEdge(PCGEx::H64U(5, 15)));

	// Test reversed order (H64U is order-independent)
	TestTrue(TEXT("Contains BA edge (reversed)"), Site.ContainsEdge(PCGEx::H64U(10, 5)));
	TestTrue(TEXT("Contains CB edge (reversed)"), Site.ContainsEdge(PCGEx::H64U(15, 10)));
	TestTrue(TEXT("Contains CA edge (reversed)"), Site.ContainsEdge(PCGEx::H64U(15, 5)));

	// Test edges that don't exist
	TestFalse(TEXT("Doesn't contain (0,1)"), Site.ContainsEdge(PCGEx::H64U(0, 1)));
	TestFalse(TEXT("Doesn't contain (5,20)"), Site.ContainsEdge(PCGEx::H64U(5, 20)));

	return true;
}

/**
 * Test FDelaunaySite2::GetSharedEdge
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite2GetSharedEdgeTest,
	"PCGEx.Unit.Delaunay.Site2.GetSharedEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite2GetSharedEdgeTest::RunTest(const FString& Parameters)
{
	// Two adjacent triangles sharing edge (1,2)
	// Triangle A: vertices 0, 1, 2
	// Triangle B: vertices 1, 2, 3
	FDelaunaySite2 SiteA(0, 1, 2, 0);
	FDelaunaySite2 SiteB(1, 2, 3, 1);

	uint64 SharedEdge = SiteA.GetSharedEdge(&SiteB);

	// The shared edge should be (1,2)
	TestEqual(TEXT("Shared edge is (1,2)"), SharedEdge, PCGEx::H64U(1, 2));

	return true;
}

/**
 * Test FDelaunaySite2::PushAdjacency
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite2PushAdjacencyTest,
	"PCGEx.Unit.Delaunay.Site2.PushAdjacency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite2PushAdjacencyTest::RunTest(const FString& Parameters)
{
	FDelaunaySite2 Site(0, 1, 2, 0);

	// Initially all neighbors are -1
	TestEqual(TEXT("Initial Neighbor[0] is -1"), Site.Neighbors[0], -1);

	// Push first adjacency
	Site.PushAdjacency(5);
	TestEqual(TEXT("First adjacency set to slot 0"), Site.Neighbors[0], 5);
	TestEqual(TEXT("Slot 1 still -1"), Site.Neighbors[1], -1);

	// Push second adjacency
	Site.PushAdjacency(10);
	TestEqual(TEXT("Second adjacency set to slot 1"), Site.Neighbors[1], 10);
	TestEqual(TEXT("Slot 2 still -1"), Site.Neighbors[2], -1);

	// Push third adjacency
	Site.PushAdjacency(15);
	TestEqual(TEXT("Third adjacency set to slot 2"), Site.Neighbors[2], 15);

	return true;
}

// =============================================================================
// FDelaunaySite3 Tests
// =============================================================================

/**
 * Test FDelaunaySite3 construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite3ConstructorTest,
	"PCGEx.Unit.Delaunay.Site3.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite3ConstructorTest::RunTest(const FString& Parameters)
{
	FIntVector4 Vertices(3, 1, 4, 2); // Will be sorted
	FDelaunaySite3 Site(Vertices, 7);

	TestEqual(TEXT("Id is set"), Site.Id, 7);

	// Vertices should be sorted
	TestTrue(TEXT("Vtx[0] <= Vtx[1]"), Site.Vtx[0] <= Site.Vtx[1]);
	TestTrue(TEXT("Vtx[1] <= Vtx[2]"), Site.Vtx[1] <= Site.Vtx[2]);
	TestTrue(TEXT("Vtx[2] <= Vtx[3]"), Site.Vtx[2] <= Site.Vtx[3]);

	// All vertices should be present
	TSet<int32> VertexSet;
	for (int i = 0; i < 4; i++) { VertexSet.Add(Site.Vtx[i]); }

	TestTrue(TEXT("Contains vertex 1"), VertexSet.Contains(1));
	TestTrue(TEXT("Contains vertex 2"), VertexSet.Contains(2));
	TestTrue(TEXT("Contains vertex 3"), VertexSet.Contains(3));
	TestTrue(TEXT("Contains vertex 4"), VertexSet.Contains(4));

	return true;
}

/**
 * Test FDelaunaySite3::ComputeFaces
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunaySite3ComputeFacesTest,
	"PCGEx.Unit.Delaunay.Site3.ComputeFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunaySite3ComputeFacesTest::RunTest(const FString& Parameters)
{
	FIntVector4 Vertices(0, 1, 2, 3);
	FDelaunaySite3 Site(Vertices, 0);

	// Initially faces should be 0
	TestEqual(TEXT("Initial Face[0] is 0"), Site.Faces[0], 0u);

	Site.ComputeFaces();

	// After computing, faces should be non-zero (they are triangle hashes)
	// Each face is a hash of 3 vertices
	TSet<uint32> FaceSet;
	for (int i = 0; i < 4; i++)
	{
		TestNotEqual(TEXT("Face is non-zero after compute"), Site.Faces[i], 0u);
		FaceSet.Add(Site.Faces[i]);
	}

	// All 4 faces should be unique (tetrahedron has 4 distinct triangular faces)
	TestEqual(TEXT("4 unique faces"), FaceSet.Num(), 4);

	return true;
}

// =============================================================================
// TDelaunay3 Tests
// =============================================================================

/**
 * Test TDelaunay3::Process with simple tetrahedron
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay3ProcessSimpleTest,
	"PCGEx.Unit.Delaunay.Delaunay3.Process.Simple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay3ProcessSimpleTest::RunTest(const FString& Parameters)
{
	// Create 4 points forming a tetrahedron
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0),
		FVector(50, 50, 100)
	};

	TDelaunay3 Delaunay;
	bool bResult = Delaunay.Process<false, false>(MakeArrayView(Positions));

	TestTrue(TEXT("Delaunay3 processes successfully"), bResult);
	TestTrue(TEXT("Delaunay3 is valid"), Delaunay.IsValid);
	TestEqual(TEXT("One tetrahedron site"), Delaunay.Sites.Num(), 1);

	// A tetrahedron has 6 edges
	TestEqual(TEXT("6 Delaunay edges"), Delaunay.DelaunayEdges.Num(), 6);

	return true;
}

/**
 * Test TDelaunay3::Process with more points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay3ProcessMultipleTest,
	"PCGEx.Unit.Delaunay.Delaunay3.Process.Multiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay3ProcessMultipleTest::RunTest(const FString& Parameters)
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

	TDelaunay3 Delaunay;
	bool bResult = Delaunay.Process<false, false>(MakeArrayView(Positions));

	TestTrue(TEXT("Delaunay3 processes cube successfully"), bResult);
	TestTrue(TEXT("Delaunay3 is valid"), Delaunay.IsValid);
	TestTrue(TEXT("Multiple tetrahedra created"), Delaunay.Sites.Num() > 1);
	TestTrue(TEXT("Has edges"), Delaunay.DelaunayEdges.Num() > 0);

	return true;
}

/**
 * Test TDelaunay3::Process with hull computation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay3ProcessWithHullTest,
	"PCGEx.Unit.Delaunay.Delaunay3.Process.WithHull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay3ProcessWithHullTest::RunTest(const FString& Parameters)
{
	// Create 5 points - tetrahedron with one point inside
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0),
		FVector(50, 50, 100),
		FVector(50, 40, 30) // Point inside
	};

	TDelaunay3 Delaunay;
	bool bResult = Delaunay.Process<false, true>(MakeArrayView(Positions));

	TestTrue(TEXT("Delaunay3 with hull processes successfully"), bResult);
	TestTrue(TEXT("Hull vertices populated"), Delaunay.DelaunayHull.Num() > 0);

	// The inner point (index 4) should not be on the hull
	TestFalse(TEXT("Inner point not on hull"), Delaunay.DelaunayHull.Contains(4));

	// Outer points (0-3) should be on hull
	TestTrue(TEXT("Vertex 0 on hull"), Delaunay.DelaunayHull.Contains(0));
	TestTrue(TEXT("Vertex 1 on hull"), Delaunay.DelaunayHull.Contains(1));
	TestTrue(TEXT("Vertex 2 on hull"), Delaunay.DelaunayHull.Contains(2));
	TestTrue(TEXT("Vertex 3 on hull"), Delaunay.DelaunayHull.Contains(3));

	return true;
}

/**
 * Test TDelaunay3::Process edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay3ProcessEdgeCasesTest,
	"PCGEx.Unit.Delaunay.Delaunay3.Process.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay3ProcessEdgeCasesTest::RunTest(const FString& Parameters)
{
	TDelaunay3 Delaunay;

	// Empty array
	TArray<FVector> Empty;
	TestFalse(TEXT("Empty array returns false"), Delaunay.Process<false, false>(MakeArrayView(Empty)));

	// Too few points (need at least 4 for 3D)
	TArray<FVector> TooFew = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};
	TestFalse(TEXT("3 points returns false"), Delaunay.Process<false, false>(MakeArrayView(TooFew)));

	return true;
}

/**
 * Test TDelaunay3::RemoveLongestEdges
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay3RemoveLongestEdgesTest,
	"PCGEx.Unit.Delaunay.Delaunay3.RemoveLongestEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay3RemoveLongestEdgesTest::RunTest(const FString& Parameters)
{
	// Create elongated tetrahedron with one clearly longest edge
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(10, 0, 0),
		FVector(5, 10, 0),
		FVector(5, 5, 10)
	};

	TDelaunay3 Delaunay;
	Delaunay.Process<false, false>(MakeArrayView(Positions));

	int32 OriginalEdgeCount = Delaunay.DelaunayEdges.Num();

	Delaunay.RemoveLongestEdges(MakeArrayView(Positions));

	TestTrue(TEXT("Some edges removed"), Delaunay.DelaunayEdges.Num() < OriginalEdgeCount);

	return true;
}

// =============================================================================
// TDelaunay2 Tests
// =============================================================================

/**
 * Test TDelaunay2::Process with simple triangle
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2ProcessSimpleTest,
	"PCGEx.Unit.Delaunay.Delaunay2.Process.Simple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2ProcessSimpleTest::RunTest(const FString& Parameters)
{
	// Create 3 points forming a triangle (minimum for Delaunay 2D)
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(50, 100, 0)
	};

	// Default projection projects onto XY plane (Z is up)
	FPCGExGeo2DProjectionDetails Projection;

	TDelaunay2 Delaunay;
	bool bResult = Delaunay.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Delaunay2 processes triangle successfully"), bResult);
	TestTrue(TEXT("Delaunay2 is valid"), Delaunay.IsValid);
	TestEqual(TEXT("One triangle site"), Delaunay.Sites.Num(), 1);
	TestEqual(TEXT("3 edges"), Delaunay.DelaunayEdges.Num(), 3);

	return true;
}

/**
 * Test TDelaunay2::Process with square (4 points)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2ProcessSquareTest,
	"PCGEx.Unit.Delaunay.Delaunay2.Process.Square",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2ProcessSquareTest::RunTest(const FString& Parameters)
{
	// Create 4 points forming a square
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TDelaunay2 Delaunay;
	bool bResult = Delaunay.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Delaunay2 processes square successfully"), bResult);
	TestTrue(TEXT("Delaunay2 is valid"), Delaunay.IsValid);
	TestEqual(TEXT("Two triangles from square"), Delaunay.Sites.Num(), 2);

	// 4 perimeter edges + 1 diagonal = 5 edges
	TestEqual(TEXT("5 edges from square"), Delaunay.DelaunayEdges.Num(), 5);

	return true;
}

/**
 * Test TDelaunay2::Process with more points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2ProcessGridTest,
	"PCGEx.Unit.Delaunay.Delaunay2.Process.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2ProcessGridTest::RunTest(const FString& Parameters)
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

	TDelaunay2 Delaunay;
	bool bResult = Delaunay.Process(MakeArrayView(Positions), Projection);

	TestTrue(TEXT("Delaunay2 processes grid successfully"), bResult);
	TestTrue(TEXT("Delaunay2 is valid"), Delaunay.IsValid);
	TestTrue(TEXT("Multiple triangles from grid"), Delaunay.Sites.Num() > 2);
	TestTrue(TEXT("Hull vertices populated"), Delaunay.DelaunayHull.Num() > 0);

	return true;
}

/**
 * Test TDelaunay2::Process edge cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2ProcessEdgeCasesTest,
	"PCGEx.Unit.Delaunay.Delaunay2.Process.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2ProcessEdgeCasesTest::RunTest(const FString& Parameters)
{
	FPCGExGeo2DProjectionDetails Projection;
	TDelaunay2 Delaunay;

	// Empty array
	TArray<FVector> Empty;
	TestFalse(TEXT("Empty array returns false"), Delaunay.Process(MakeArrayView(Empty), Projection));

	// Too few points (need at least 3 for 2D)
	TArray<FVector> TooFew = {
		FVector(0, 0, 0),
		FVector(100, 0, 0)
	};
	TestFalse(TEXT("2 points returns false"), Delaunay.Process(MakeArrayView(TooFew), Projection));

	return true;
}

/**
 * Test TDelaunay2::RemoveLongestEdges
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2RemoveLongestEdgesTest,
	"PCGEx.Unit.Delaunay.Delaunay2.RemoveLongestEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2RemoveLongestEdgesTest::RunTest(const FString& Parameters)
{
	// Create square with diagonal being longest edge in one triangle
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0)
	};

	FPCGExGeo2DProjectionDetails Projection;

	TDelaunay2 Delaunay;
	Delaunay.Process(MakeArrayView(Positions), Projection);

	int32 OriginalEdgeCount = Delaunay.DelaunayEdges.Num();

	Delaunay.RemoveLongestEdges(MakeArrayView(Positions));

	TestTrue(TEXT("Some edges removed"), Delaunay.DelaunayEdges.Num() < OriginalEdgeCount);

	return true;
}

/**
 * Test TDelaunay2 hull detection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDelaunay2HullTest,
	"PCGEx.Unit.Delaunay.Delaunay2.Hull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDelaunay2HullTest::RunTest(const FString& Parameters)
{
	// Create points with one clearly inside
	TArray<FVector> Positions = {
		FVector(0, 0, 0),
		FVector(100, 0, 0),
		FVector(100, 100, 0),
		FVector(0, 100, 0),
		FVector(50, 50, 0) // Center point, not on hull
	};

	FPCGExGeo2DProjectionDetails Projection;

	TDelaunay2 Delaunay;
	Delaunay.Process(MakeArrayView(Positions), Projection);

	// Corner points should be on hull
	TestTrue(TEXT("Vertex 0 on hull"), Delaunay.DelaunayHull.Contains(0));
	TestTrue(TEXT("Vertex 1 on hull"), Delaunay.DelaunayHull.Contains(1));
	TestTrue(TEXT("Vertex 2 on hull"), Delaunay.DelaunayHull.Contains(2));
	TestTrue(TEXT("Vertex 3 on hull"), Delaunay.DelaunayHull.Contains(3));

	// Center point should not be on hull
	TestFalse(TEXT("Center point not on hull"), Delaunay.DelaunayHull.Contains(4));

	return true;
}
