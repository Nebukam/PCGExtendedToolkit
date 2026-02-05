// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExOBB Unit Tests
 *
 * Tests OBB (Oriented Bounding Box) utilities:
 * - OBB construction from transforms, AABBs
 * - Point-in-box tests
 * - Sphere overlap tests
 * - SAT (Separating Axis Theorem) overlap tests
 * - Signed distance calculations
 * - Closest point on OBB surface
 *
 * These are pure geometry tests - no game world required.
 *
 * Test naming convention: PCGEx.Unit.OBB.<Category>
 */

#include "Misc/AutomationTest.h"
#include "Math/OBB/PCGExOBB.h"
#include "Math/OBB/PCGExOBBTests.h"

using namespace PCGExMath::OBB;

// =============================================================================
// OBB Factory Tests
// =============================================================================

/**
 * Test OBB construction from transforms
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBFactoryFromTransformTest,
	"PCGEx.Unit.OBB.Factory.FromTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBFactoryFromTransformTest::RunTest(const FString& Parameters)
{
	// Create a simple axis-aligned OBB at origin
	FTransform IdentityTransform = FTransform::Identity;
	FVector Extents(50.0f, 50.0f, 50.0f);
	FOBB Box = Factory::FromTransform(IdentityTransform, Extents, 0);

	TestTrue(TEXT("Origin at identity transform location"),
	         Box.GetOrigin().Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Extents preserved"),
	         Box.GetExtents().Equals(Extents, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Index preserved"), Box.GetIndex(), 0);

	// Create rotated OBB
	FTransform RotatedTransform(FRotator(0, 45, 0), FVector(100, 200, 300));
	FOBB RotatedBox = Factory::FromTransform(RotatedTransform, Extents, 1);

	TestTrue(TEXT("Origin at transform location"),
	         RotatedBox.GetOrigin().Equals(FVector(100, 200, 300), KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Index preserved for rotated"), RotatedBox.GetIndex(), 1);

	return true;
}

/**
 * Test OBB construction from AABB
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBFactoryFromAABBTest,
	"PCGEx.Unit.OBB.Factory.FromAABB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBFactoryFromAABBTest::RunTest(const FString& Parameters)
{
	// Create AABB from min/max
	FBox WorldBox(FVector(-100, -50, -25), FVector(100, 50, 25));
	FOBB Box = Factory::FromAABB(WorldBox, 42);

	// Center should be at box center
	FVector ExpectedCenter = WorldBox.GetCenter();
	TestTrue(TEXT("Origin at AABB center"),
	         Box.GetOrigin().Equals(ExpectedCenter, KINDA_SMALL_NUMBER));

	// Extents should be half-size
	FVector ExpectedExtents = WorldBox.GetExtent();
	TestTrue(TEXT("Extents are AABB half-extents"),
	         Box.GetExtents().Equals(ExpectedExtents, KINDA_SMALL_NUMBER));

	TestEqual(TEXT("Index preserved"), Box.GetIndex(), 42);

	// Rotation should be identity (axis-aligned)
	TestTrue(TEXT("AABB produces identity rotation"),
	         Box.GetRotation().Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test OBB expansion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBFactoryExpandedTest,
	"PCGEx.Unit.OBB.Factory.Expanded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBFactoryExpandedTest::RunTest(const FString& Parameters)
{
	FVector OriginalExtents(50.0f, 50.0f, 50.0f);
	FOBB Original = Factory::FromTransform(FTransform::Identity, OriginalExtents, 0);

	float Expansion = 10.0f;
	FOBB Expanded = Factory::Expanded(Original, Expansion);

	FVector ExpectedExtents = OriginalExtents + FVector(Expansion);
	TestTrue(TEXT("Extents increased by expansion"),
	         Expanded.GetExtents().Equals(ExpectedExtents, KINDA_SMALL_NUMBER));

	// Origin should be unchanged
	TestTrue(TEXT("Origin unchanged after expansion"),
	         Expanded.GetOrigin().Equals(Original.GetOrigin(), KINDA_SMALL_NUMBER));

	// Radius should be updated
	TestTrue(TEXT("Radius updated after expansion"),
	         Expanded.GetRadius() > Original.GetRadius());

	return true;
}

// =============================================================================
// Point Inside Tests
// =============================================================================

/**
 * Test point-in-box for axis-aligned OBB
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBPointInsideAxisAlignedTest,
	"PCGEx.Unit.OBB.PointInside.AxisAligned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBPointInsideAxisAlignedTest::RunTest(const FString& Parameters)
{
	// Create 100x100x100 box centered at origin
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Point at center
	TestTrue(TEXT("Center point is inside"),
	         PointInside(Box, FVector::ZeroVector));

	// Points inside
	TestTrue(TEXT("Point (25,25,25) is inside"),
	         PointInside(Box, FVector(25, 25, 25)));
	TestTrue(TEXT("Point (-49,-49,-49) is inside"),
	         PointInside(Box, FVector(-49, -49, -49)));

	// Points on boundary (should be inside due to <= comparison)
	TestTrue(TEXT("Point on X boundary is inside"),
	         PointInside(Box, FVector(50, 0, 0)));

	// Points outside
	TestFalse(TEXT("Point (51,0,0) is outside"),
	          PointInside(Box, FVector(51, 0, 0)));
	TestFalse(TEXT("Point (0,51,0) is outside"),
	          PointInside(Box, FVector(0, 51, 0)));
	TestFalse(TEXT("Point (0,0,51) is outside"),
	          PointInside(Box, FVector(0, 0, 51)));
	TestFalse(TEXT("Point (100,100,100) is outside"),
	          PointInside(Box, FVector(100, 100, 100)));

	return true;
}

/**
 * Test point-in-box for rotated OBB
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBPointInsideRotatedTest,
	"PCGEx.Unit.OBB.PointInside.Rotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBPointInsideRotatedTest::RunTest(const FString& Parameters)
{
	// Create box rotated 45 degrees around Z axis
	// A 100x50 box rotated 45 degrees creates a diamond shape in XY plane
	FTransform RotatedTransform(FRotator(0, 45, 0), FVector::ZeroVector);
	FOBB Box = Factory::FromTransform(RotatedTransform, FVector(50, 25, 25), 0);

	// Center should still be inside
	TestTrue(TEXT("Center point is inside rotated box"),
	         PointInside(Box, FVector::ZeroVector));

	// Point along rotated X axis (should be inside)
	FVector RotatedXPoint = FRotator(0, 45, 0).RotateVector(FVector(40, 0, 0));
	TestTrue(TEXT("Point along rotated X axis is inside"),
	         PointInside(Box, RotatedXPoint));

	// Point that would be inside axis-aligned but outside rotated
	// At 45 degrees, point (x,x,0) transforms to local space as (x*sqrt(2), 0, 0)
	// Box X extent is 50, so x*sqrt(2) > 50 means x > 35.36
	// Use (40, 40, 0) which gives local x ≈ 56.6 > 50, so it's outside
	TestFalse(TEXT("Point (40,40,0) is outside rotated box"),
	          PointInside(Box, FVector(40, 40, 0)));

	return true;
}

/**
 * Test point-in-box with expansion
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBPointInsideExpandedTest,
	"PCGEx.Unit.OBB.PointInside.Expanded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBPointInsideExpandedTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Point just outside normal box
	FVector TestPoint(55, 0, 0);
	TestFalse(TEXT("Point outside unexpanded box"),
	          PointInside(Box, TestPoint));

	// Same point with expansion should be inside
	TestTrue(TEXT("Point inside expanded box"),
	         PointInsideExpanded(Box.Bounds, Box.Orientation, TestPoint, 10.0f));

	return true;
}

// =============================================================================
// Sphere Overlap Tests
// =============================================================================

/**
 * Test sphere-sphere overlap via OBB bounds
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSphereOverlapTest,
	"PCGEx.Unit.OBB.SphereOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSphereOverlapTest::RunTest(const FString& Parameters)
{
	// Create two OBBs with known radii
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	// BoxA radius = magnitude(50,50,50) ≈ 86.6

	// Create BoxB nearby
	FTransform TransformB(FRotator::ZeroRotator, FVector(100, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// These should overlap (combined radii > distance)
	TestTrue(TEXT("Nearby boxes have sphere overlap"),
	         SphereOverlap(BoxA.Bounds, BoxB.Bounds));

	// Create BoxC far away
	FTransform TransformC(FRotator::ZeroRotator, FVector(1000, 0, 0));
	FOBB BoxC = Factory::FromTransform(TransformC, FVector(50.0f), 2);

	// No sphere overlap
	TestFalse(TEXT("Distant boxes have no sphere overlap"),
	          SphereOverlap(BoxA.Bounds, BoxC.Bounds));

	// Test point-sphere overlap
	TestTrue(TEXT("Point inside sphere radius overlaps"),
	         SphereOverlap(BoxA.Bounds, FVector(50, 0, 0), 50.0f));
	TestFalse(TEXT("Point outside combined radius doesn't overlap"),
	          SphereOverlap(BoxA.Bounds, FVector(200, 0, 0), 10.0f));

	return true;
}

/**
 * Test sphere containment
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSphereContainsTest,
	"PCGEx.Unit.OBB.SphereContains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSphereContainsTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(100.0f), 0);
	// Box radius ≈ 173.2

	// Small sphere at center should be contained
	TestTrue(TEXT("Small centered sphere is contained"),
	         SphereContains(Box.Bounds, FVector::ZeroVector, 10.0f));

	// Sphere at edge, if sphere radius + distance > container radius, not contained
	TestFalse(TEXT("Sphere at edge extending outside is not contained"),
	          SphereContains(Box.Bounds, FVector(150, 0, 0), 50.0f));

	// Sphere entirely inside
	TestTrue(TEXT("Sphere entirely inside is contained"),
	         SphereContains(Box.Bounds, FVector(50, 0, 0), 20.0f));

	return true;
}

// =============================================================================
// SAT Overlap Tests
// =============================================================================

/**
 * Test SAT (Separating Axis Theorem) overlap for aligned boxes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSATOverlapAlignedTest,
	"PCGEx.Unit.OBB.SATOverlap.Aligned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSATOverlapAlignedTest::RunTest(const FString& Parameters)
{
	// Two overlapping axis-aligned boxes
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FTransform TransformB(FRotator::ZeroRotator, FVector(75, 0, 0)); // Overlaps by 25 units
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	TestTrue(TEXT("Overlapping aligned boxes detected by SAT"),
	         SATOverlap(BoxA, BoxB));

	// Non-overlapping boxes
	FTransform TransformC(FRotator::ZeroRotator, FVector(150, 0, 0)); // Gap of 50 units
	FOBB BoxC = Factory::FromTransform(TransformC, FVector(50.0f), 2);

	TestFalse(TEXT("Separated aligned boxes detected by SAT"),
	          SATOverlap(BoxA, BoxC));

	return true;
}

/**
 * Test SAT overlap for rotated boxes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSATOverlapRotatedTest,
	"PCGEx.Unit.OBB.SATOverlap.Rotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSATOverlapRotatedTest::RunTest(const FString& Parameters)
{
	// Create two boxes with different rotations
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Rotated box at the same position - must overlap
	FTransform RotatedTransform(FRotator(30, 45, 15), FVector::ZeroVector);
	FOBB BoxB = Factory::FromTransform(RotatedTransform, FVector(50.0f), 1);

	TestTrue(TEXT("Concentric rotated boxes overlap"),
	         SATOverlap(BoxA, BoxB));

	// Rotated box moved away but still overlapping
	FTransform RotatedNearby(FRotator(30, 45, 15), FVector(70, 0, 0));
	FOBB BoxC = Factory::FromTransform(RotatedNearby, FVector(50.0f), 2);

	TestTrue(TEXT("Nearby rotated boxes overlap"),
	         SATOverlap(BoxA, BoxC));

	// Rotated box clearly separated
	FTransform RotatedFar(FRotator(30, 45, 15), FVector(200, 0, 0));
	FOBB BoxD = Factory::FromTransform(RotatedFar, FVector(50.0f), 3);

	TestFalse(TEXT("Distant rotated boxes don't overlap"),
	          SATOverlap(BoxA, BoxD));

	return true;
}

// =============================================================================
// Signed Distance Tests
// =============================================================================

/**
 * Test signed distance to OBB surface
 *
 * Negative = inside, Positive = outside
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSignedDistanceTest,
	"PCGEx.Unit.OBB.SignedDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSignedDistanceTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Point at center - should be deeply inside (negative)
	float CenterDist = SignedDistance(Box, FVector::ZeroVector);
	TestTrue(TEXT("Center point has negative signed distance"),
	         CenterDist < 0.0f);
	TestTrue(TEXT("Center distance is -50 (distance to nearest face)"),
	         FMath::IsNearlyEqual(CenterDist, -50.0f, 0.1f));

	// Point on surface - should be zero
	float SurfaceDist = SignedDistance(Box, FVector(50, 0, 0));
	TestTrue(TEXT("Surface point has near-zero signed distance"),
	         FMath::IsNearlyEqual(SurfaceDist, 0.0f, KINDA_SMALL_NUMBER));

	// Point outside - should be positive
	float OutsideDist = SignedDistance(Box, FVector(60, 0, 0));
	TestTrue(TEXT("Outside point has positive signed distance"),
	         OutsideDist > 0.0f);
	TestTrue(TEXT("Outside distance is 10 units"),
	         FMath::IsNearlyEqual(OutsideDist, 10.0f, 0.1f));

	// Point inside but off-center
	float InsideDist = SignedDistance(Box, FVector(40, 0, 0));
	TestTrue(TEXT("Off-center inside point has negative distance"),
	         InsideDist < 0.0f);
	TestTrue(TEXT("Distance is -10 (to nearest face)"),
	         FMath::IsNearlyEqual(InsideDist, -10.0f, 0.1f));

	return true;
}

// =============================================================================
// Closest Point Tests
// =============================================================================

/**
 * Test closest point on OBB surface
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBClosestPointTest,
	"PCGEx.Unit.OBB.ClosestPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBClosestPointTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Point outside along X axis
	FVector Closest = ClosestPoint(Box, FVector(100, 0, 0));
	TestTrue(TEXT("Closest point on +X face"),
	         Closest.Equals(FVector(50, 0, 0), KINDA_SMALL_NUMBER));

	// Point outside along diagonal
	Closest = ClosestPoint(Box, FVector(100, 100, 100));
	TestTrue(TEXT("Closest point at corner"),
	         Closest.Equals(FVector(50, 50, 50), KINDA_SMALL_NUMBER));

	// Point inside - closest point is the point itself clamped to surface
	Closest = ClosestPoint(Box, FVector(25, 0, 0));
	// When inside, the point is clamped to the box, which for an interior point
	// returns the clamped position (the point itself if inside)
	TestTrue(TEXT("Inside point clamps to itself"),
	         Closest.Equals(FVector(25, 0, 0), KINDA_SMALL_NUMBER));

	// Point at center
	Closest = ClosestPoint(Box, FVector::ZeroVector);
	TestTrue(TEXT("Center point clamps to origin"),
	         Closest.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// TestPoint with Mode Tests
// =============================================================================

/**
 * Test TestPoint with different modes
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTestPointModeTest,
	"PCGEx.Unit.OBB.TestPoint.Modes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTestPointModeTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Point just outside box but inside bounding sphere
	// Box extents = 50, radius ≈ 86.6
	FVector TestPointNear(60, 0, 0);

	// Box mode - outside
	TestFalse(TEXT("Point outside box fails Box mode"),
	          TestPoint(Box, TestPointNear, EPCGExBoxCheckMode::Box));

	// Sphere mode - inside bounding sphere
	TestTrue(TEXT("Point inside sphere passes Sphere mode"),
	         TestPoint(Box, TestPointNear, EPCGExBoxCheckMode::Sphere));

	// ExpandedBox mode with 15 unit expansion - now inside
	TestTrue(TEXT("Point inside expanded box passes ExpandedBox mode"),
	         TestPoint(Box, TestPointNear, EPCGExBoxCheckMode::ExpandedBox, 15.0f));

	// Point far outside
	FVector TestPointFar(200, 0, 0);
	TestFalse(TEXT("Distant point fails all modes"),
	          TestPoint(Box, TestPointFar, EPCGExBoxCheckMode::Sphere));

	return true;
}

// =============================================================================
// Local/World Transform Tests
// =============================================================================

/**
 * Test local/world coordinate transformations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTransformTest,
	"PCGEx.Unit.OBB.Transform.LocalWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTransformTest::RunTest(const FString& Parameters)
{
	// Create rotated and translated OBB
	FTransform Transform(FRotator(0, 90, 0), FVector(100, 200, 300));
	FOBB Box = Factory::FromTransform(Transform, FVector(50.0f), 0);

	// Test round-trip: world -> local -> world
	FVector WorldPoint(150, 200, 300);
	FVector LocalPoint = Box.ToLocal(WorldPoint);
	FVector BackToWorld = Box.ToWorld(LocalPoint);

	TestTrue(TEXT("Round-trip world->local->world preserves point"),
	         BackToWorld.Equals(WorldPoint, KINDA_SMALL_NUMBER));

	// Test that local coordinates make sense
	// 90 degree yaw rotation: world X becomes local -Y, world Y becomes local X
	FVector OriginLocal = Box.ToLocal(Box.GetOrigin());
	TestTrue(TEXT("Origin transforms to local zero"),
	         OriginLocal.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// TestOverlap (OBB-OBB) Tests
// =============================================================================

/**
 * Test TestOverlap with Box mode
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTestOverlapBoxModeTest,
	"PCGEx.Unit.OBB.TestOverlap.BoxMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTestOverlapBoxModeTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Overlapping box
	FTransform TransformB(FRotator::ZeroRotator, FVector(75, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	TestTrue(TEXT("Overlapping boxes detected in Box mode"),
	         TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::Box));

	// Non-overlapping box
	FTransform TransformC(FRotator::ZeroRotator, FVector(150, 0, 0));
	FOBB BoxC = Factory::FromTransform(TransformC, FVector(50.0f), 2);

	TestFalse(TEXT("Separated boxes not detected in Box mode"),
	          TestOverlap(BoxA, BoxC, EPCGExBoxCheckMode::Box));

	return true;
}

/**
 * Test TestOverlap with Sphere mode
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTestOverlapSphereModeTest,
	"PCGEx.Unit.OBB.TestOverlap.SphereMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTestOverlapSphereModeTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	// BoxA radius ≈ 86.6

	// Box that doesn't overlap as box but sphere overlaps
	// Place at distance ~150 (less than 2*86.6 = 173.2)
	FTransform TransformB(FRotator::ZeroRotator, FVector(150, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// Box mode should NOT detect overlap (gap exists)
	TestFalse(TEXT("Boxes don't overlap in Box mode"),
	          TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::Box));

	// Sphere mode SHOULD detect overlap (spheres overlap)
	TestTrue(TEXT("Spheres overlap in Sphere mode"),
	         TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::Sphere));

	return true;
}

/**
 * Test TestOverlap with ExpandedBox mode
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTestOverlapExpandedBoxModeTest,
	"PCGEx.Unit.OBB.TestOverlap.ExpandedBoxMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTestOverlapExpandedBoxModeTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Box just outside normal overlap range
	FTransform TransformB(FRotator::ZeroRotator, FVector(110, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// Normal box mode - no overlap (gap of 10 units)
	TestFalse(TEXT("Boxes don't overlap without expansion"),
	          TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::Box));

	// Expanded by 15 - should overlap
	TestTrue(TEXT("Boxes overlap with 15 unit expansion"),
	         TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::ExpandedBox, 15.0f));

	return true;
}

/**
 * Test TestOverlap with ExpandedSphere mode
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBTestOverlapExpandedSphereModeTest,
	"PCGEx.Unit.OBB.TestOverlap.ExpandedSphereMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBTestOverlapExpandedSphereModeTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	// BoxA radius ≈ 86.6

	// Box far enough that even spheres don't overlap
	// Distance = 200, combined radius = 2*86.6 ≈ 173.2, so no overlap
	FTransform TransformB(FRotator::ZeroRotator, FVector(200, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// Normal sphere mode - no overlap
	TestFalse(TEXT("Spheres don't overlap at distance 200"),
	          TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::Sphere));

	// Expanded sphere by 30 - should overlap (173.2 + 30 = 203.2 > 200)
	TestTrue(TEXT("Expanded spheres overlap with 30 unit expansion"),
	         TestOverlap(BoxA, BoxB, EPCGExBoxCheckMode::ExpandedSphere, 30.0f));

	return true;
}

// =============================================================================
// Policy Class Tests
// =============================================================================

/**
 * Test FPolicy runtime class with different modes
 * Note: TPolicy<Mode> template aliases (FPolicyBox, etc.) are not tested directly
 * because they require explicit DLL export instantiation. FPolicy provides the
 * same functionality with runtime mode selection.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBPolicyModesTest,
	"PCGEx.Unit.OBB.Policy.Modes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBPolicyModesTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	FTransform TransformB(FRotator::ZeroRotator, FVector(75, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// Test Box mode (equivalent to FPolicyBox)
	FPolicy PolicyBox(EPCGExBoxCheckMode::Box);
	TestTrue(TEXT("Box mode detects overlapping boxes"),
	         PolicyBox.TestOverlap(BoxA, BoxB));
	TestTrue(TEXT("Box mode detects point inside box"),
	         PolicyBox.TestPoint(BoxA, FVector::ZeroVector));
	TestFalse(TEXT("Box mode detects point outside box"),
	          PolicyBox.TestPoint(BoxA, FVector(100, 0, 0)));

	// Test Sphere mode (equivalent to FPolicySphere)
	FPolicy PolicySphere(EPCGExBoxCheckMode::Sphere);
	TestTrue(TEXT("Sphere mode detects point inside sphere"),
	         PolicySphere.TestPoint(BoxA, FVector(60, 0, 0)));

	// Test ExpandedBox mode with expansion (equivalent to FPolicyExpandedBox)
	FPolicy PolicyExpanded(EPCGExBoxCheckMode::ExpandedBox, 15.0f);
	TestTrue(TEXT("ExpandedBox mode detects point in expanded box"),
	         PolicyExpanded.TestPoint(BoxA, FVector(60, 0, 0)));

	return true;
}

/**
 * Test FPolicy runtime class
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBFPolicyTest,
	"PCGEx.Unit.OBB.Policy.FPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBFPolicyTest::RunTest(const FString& Parameters)
{
	FOBB BoxA = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	FTransform TransformB(FRotator::ZeroRotator, FVector(75, 0, 0));
	FOBB BoxB = Factory::FromTransform(TransformB, FVector(50.0f), 1);

	// Default construction
	FPolicy DefaultPolicy;
	TestEqual(TEXT("Default mode is Box"),
	          DefaultPolicy.Mode, EPCGExBoxCheckMode::Box);
	TestTrue(TEXT("Default expansion is 0"),
	         FMath::IsNearlyEqual(DefaultPolicy.Expansion, 0.0f, KINDA_SMALL_NUMBER));

	// Parameterized construction
	FPolicy ExpandedPolicy(EPCGExBoxCheckMode::ExpandedBox, 20.0f);
	TestEqual(TEXT("Mode set correctly"),
	          ExpandedPolicy.Mode, EPCGExBoxCheckMode::ExpandedBox);
	TestTrue(TEXT("Expansion set correctly"),
	         FMath::IsNearlyEqual(ExpandedPolicy.Expansion, 20.0f, KINDA_SMALL_NUMBER));

	// Test functionality
	FVector PointJustOutside(55, 0, 0);
	FPolicy BoxPolicy(EPCGExBoxCheckMode::Box);
	FPolicy SpherePolicy(EPCGExBoxCheckMode::Sphere);

	TestFalse(TEXT("Box policy rejects point outside box"),
	          BoxPolicy.TestPoint(BoxA, PointJustOutside));
	TestTrue(TEXT("Sphere policy accepts point inside sphere"),
	         SpherePolicy.TestPoint(BoxA, PointJustOutside));

	// Test overlap
	TestTrue(TEXT("FPolicy.TestOverlap works for overlapping boxes"),
	         BoxPolicy.TestOverlap(BoxA, BoxB));

	return true;
}
