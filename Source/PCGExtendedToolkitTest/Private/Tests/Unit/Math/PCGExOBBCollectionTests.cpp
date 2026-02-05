// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Math/OBB/PCGExOBBCollection.h"
#include "Math/OBB/PCGExOBB.h"

//////////////////////////////////////////////////////////////////
// FCollection Construction Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionDefaultState,
	"PCGEx.Unit.Math.OBBCollection.Construction.DefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionDefaultState::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	TestEqual(TEXT("CloudIndex default to -1"), Collection.CloudIndex, -1);
	TestEqual(TEXT("Num returns 0"), Collection.Num(), 0);
	TestTrue(TEXT("IsEmpty returns true"), Collection.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionReserve,
	"PCGEx.Unit.Math.OBBCollection.Construction.Reserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionReserve::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Reserve(100);

	// Reserve doesn't change Num, just pre-allocates
	TestEqual(TEXT("Num is still 0 after reserve"), Collection.Num(), 0);
	TestTrue(TEXT("IsEmpty is still true"), Collection.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionReset,
	"PCGEx.Unit.Math.OBBCollection.Construction.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionReset::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	// Add some boxes
	const FBox LocalBox(FVector(-10, -10, -10), FVector(10, 10, 10));
	Collection.Add(Factory::FromAABB(LocalBox, 0));
	Collection.Add(Factory::FromAABB(LocalBox, 1));
	Collection.BuildOctree();

	TestEqual(TEXT("Has 2 items before reset"), Collection.Num(), 2);

	Collection.Reset();

	TestEqual(TEXT("Num is 0 after reset"), Collection.Num(), 0);
	TestTrue(TEXT("IsEmpty after reset"), Collection.IsEmpty());

	return true;
}

//////////////////////////////////////////////////////////////////
// Adding OBBs Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionAddOBB,
	"PCGEx.Unit.Math.OBBCollection.Add.OBB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionAddOBB::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 42);

	Collection.Add(Box);

	TestEqual(TEXT("Num is 1"), Collection.Num(), 1);
	TestFalse(TEXT("IsEmpty is false"), Collection.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionAddTransform,
	"PCGEx.Unit.Math.OBBCollection.Add.Transform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionAddTransform::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	const FTransform Transform(FVector(100, 200, 300));
	const FBox LocalBox(FVector(-25, -25, -25), FVector(25, 25, 25));

	Collection.Add(Transform, LocalBox, 99);

	TestEqual(TEXT("Num is 1"), Collection.Num(), 1);

	// Verify we can retrieve it
	const FBounds& Bounds = Collection.GetBounds(0);
	TestTrue(TEXT("Origin matches transform location"), Bounds.Origin.Equals(FVector(100, 200, 300), 0.1f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionAddMultiple,
	"PCGEx.Unit.Math.OBBCollection.Add.Multiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionAddMultiple::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	for (int32 i = 0; i < 10; i++)
	{
		const FBox LocalBox(FVector(-10, -10, -10), FVector(10, 10, 10));
		const FTransform Transform(FVector(i * 100.0f, 0, 0));
		Collection.Add(Transform, LocalBox, i);
	}

	TestEqual(TEXT("Num is 10"), Collection.Num(), 10);

	// Verify indices
	for (int32 i = 0; i < 10; i++)
	{
		const FBounds& Bounds = Collection.GetBounds(i);
		TestEqual(FString::Printf(TEXT("Box %d has correct index"), i), Bounds.Index, i);
	}

	return true;
}

//////////////////////////////////////////////////////////////////
// GetOBB and Accessors Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionGetOBB,
	"PCGEx.Unit.Math.OBBCollection.Accessors.GetOBB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionGetOBB::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	const FTransform Transform(FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0f)), FVector(100, 100, 0));
	const FBox LocalBox(FVector(-30, -30, -30), FVector(30, 30, 30));

	Collection.Add(Transform, LocalBox, 7);
	Collection.BuildOctree();

	const FOBB RetrievedBox = Collection.GetOBB(0);

	TestEqual(TEXT("Index matches"), RetrievedBox.Bounds.Index, 7);
	TestTrue(TEXT("Origin matches"), RetrievedBox.Bounds.Origin.Equals(FVector(100, 100, 0), 0.1f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionGetWorldBounds,
	"PCGEx.Unit.Math.OBBCollection.Accessors.GetWorldBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionGetWorldBounds::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	// Add boxes at different positions
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-10, -10, -10), FVector(10, 10, 10)), 0);
	Collection.Add(FTransform(FVector(100, 0, 0)), FBox(FVector(-10, -10, -10), FVector(10, 10, 10)), 1);
	Collection.BuildOctree();

	const FBox& WorldBounds = Collection.GetWorldBounds();

	// World bounds should encompass both boxes
	// First box: -10 to 10 on all axes
	// Second box: 90 to 110 on X, -10 to 10 on Y and Z
	TestTrue(TEXT("World bounds Min.X covers first box"), WorldBounds.Min.X <= -10);
	TestTrue(TEXT("World bounds Max.X covers second box"), WorldBounds.Max.X >= 110);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionGetArrays,
	"PCGEx.Unit.Math.OBBCollection.Accessors.GetArrays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionGetArrays::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	for (int32 i = 0; i < 5; i++)
	{
		Collection.Add(FTransform(FVector(i * 50.0f, 0, 0)), FBox(FVector(-10, -10, -10), FVector(10, 10, 10)), i);
	}

	const TArray<FBounds>& Bounds = Collection.GetBoundsArray();
	const TArray<FOrientation>& Orientations = Collection.GetOrientationsArray();

	TestEqual(TEXT("Bounds array size matches"), Bounds.Num(), 5);
	TestEqual(TEXT("Orientations array size matches"), Orientations.Num(), 5);

	return true;
}

//////////////////////////////////////////////////////////////////
// Point Query Tests (require octree)
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionIsPointInsideNoOctree,
	"PCGEx.Unit.Math.OBBCollection.PointQueries.NoOctree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionIsPointInsideNoOctree::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	// Don't build octree

	// Should return false when no octree
	const bool bResult = Collection.IsPointInside(FVector::ZeroVector);
	TestFalse(TEXT("IsPointInside returns false without octree"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionIsPointInsideSingleBox,
	"PCGEx.Unit.Math.OBBCollection.PointQueries.SingleBox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionIsPointInsideSingleBox::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	TestTrue(TEXT("Point at center is inside"), Collection.IsPointInside(FVector::ZeroVector));
	TestTrue(TEXT("Point at edge is inside"), Collection.IsPointInside(FVector(49, 0, 0)));
	TestFalse(TEXT("Point far outside is not inside"), Collection.IsPointInside(FVector(200, 0, 0)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionIsPointInsideWithIndex,
	"PCGEx.Unit.Math.OBBCollection.PointQueries.WithIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionIsPointInsideWithIndex::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	// Add two separate boxes
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-20, -20, -20), FVector(20, 20, 20)), 0);
	Collection.Add(FTransform(FVector(100, 0, 0)), FBox(FVector(-20, -20, -20), FVector(20, 20, 20)), 1);
	Collection.BuildOctree();

	int32 FoundIndex = -1;

	// Point in first box
	TestTrue(TEXT("Point in first box is inside"), Collection.IsPointInside(FVector(0, 0, 0), FoundIndex));
	TestEqual(TEXT("Found index is 0"), FoundIndex, 0);

	// Point in second box
	FoundIndex = -1;
	TestTrue(TEXT("Point in second box is inside"), Collection.IsPointInside(FVector(100, 0, 0), FoundIndex));
	TestEqual(TEXT("Found index is 1"), FoundIndex, 1);

	// Point outside both
	FoundIndex = -1;
	TestFalse(TEXT("Point outside is not inside"), Collection.IsPointInside(FVector(50, 0, 0), FoundIndex));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionFindContaining,
	"PCGEx.Unit.Math.OBBCollection.PointQueries.FindContaining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionFindContaining::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;

	// Add overlapping boxes
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.Add(FTransform(FVector(30, 0, 0)), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 1);
	Collection.BuildOctree();

	TArray<int32> ContainingIndices;

	// Point in overlap region (both boxes)
	Collection.FindContaining(FVector(15, 0, 0), ContainingIndices);
	TestEqual(TEXT("Point in overlap is in 2 boxes"), ContainingIndices.Num(), 2);

	// Point only in first box
	ContainingIndices.Empty();
	Collection.FindContaining(FVector(-40, 0, 0), ContainingIndices);
	TestEqual(TEXT("Point in first box only"), ContainingIndices.Num(), 1);
	if (ContainingIndices.Num() > 0)
	{
		TestEqual(TEXT("First box index is 0"), ContainingIndices[0], 0);
	}

	return true;
}

//////////////////////////////////////////////////////////////////
// OBB-OBB Overlap Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionOverlapsNoOctree,
	"PCGEx.Unit.Math.OBBCollection.OBBQueries.OverlapsNoOctree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionOverlapsNoOctree::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	// Don't build octree

	const FOBB Query = Factory::FromAABB(FBox(FVector(-10, -10, -10), FVector(10, 10, 10)), -1);
	TestFalse(TEXT("Overlaps returns false without octree"), Collection.Overlaps(Query));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionOverlaps,
	"PCGEx.Unit.Math.OBBCollection.OBBQueries.Overlaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionOverlaps::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	// Query that overlaps
	const FOBB OverlappingQuery = Factory::FromAABB(FBox(FVector(40, -10, -10), FVector(60, 10, 10)), -1);
	TestTrue(TEXT("Overlapping query returns true"), Collection.Overlaps(OverlappingQuery));

	// Query that doesn't overlap
	const FOBB NonOverlappingQuery = Factory::FromAABB(FBox(FVector(100, -10, -10), FVector(120, 10, 10)), -1);
	TestFalse(TEXT("Non-overlapping query returns false"), Collection.Overlaps(NonOverlappingQuery));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionFindFirstOverlap,
	"PCGEx.Unit.Math.OBBCollection.OBBQueries.FindFirstOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionFindFirstOverlap::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-30, -30, -30), FVector(30, 30, 30)), 0);
	Collection.Add(FTransform(FVector(100, 0, 0)), FBox(FVector(-30, -30, -30), FVector(30, 30, 30)), 1);
	Collection.BuildOctree();

	int32 FoundIndex = -1;

	// Query overlapping second box
	const FOBB Query = Factory::FromAABB(FBox(FVector(90, -10, -10), FVector(110, 10, 10)), -1);
	TestTrue(TEXT("Found overlap"), Collection.FindFirstOverlap(Query, FoundIndex));
	TestEqual(TEXT("Found index is 1"), FoundIndex, 1);

	// Query not overlapping
	const FOBB NoOverlapQuery = Factory::FromAABB(FBox(FVector(200, -10, -10), FVector(220, 10, 10)), -1);
	FoundIndex = -1;
	TestFalse(TEXT("No overlap found"), Collection.FindFirstOverlap(NoOverlapQuery, FoundIndex));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionFindAllOverlaps,
	"PCGEx.Unit.Math.OBBCollection.OBBQueries.FindAllOverlaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionFindAllOverlaps::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	// Three boxes in a row, close together
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-30, -30, -30), FVector(30, 30, 30)), 0);
	Collection.Add(FTransform(FVector(50, 0, 0)), FBox(FVector(-30, -30, -30), FVector(30, 30, 30)), 1);
	Collection.Add(FTransform(FVector(100, 0, 0)), FBox(FVector(-30, -30, -30), FVector(30, 30, 30)), 2);
	Collection.BuildOctree();

	TArray<int32> OverlapIndices;

	// Large query overlapping all three
	const FOBB BigQuery = Factory::FromAABB(FBox(FVector(-50, -50, -50), FVector(150, 50, 50)), -1);
	Collection.FindAllOverlaps(BigQuery, OverlapIndices);
	TestEqual(TEXT("Big query overlaps all 3 boxes"), OverlapIndices.Num(), 3);

	// Small query overlapping only middle box
	OverlapIndices.Empty();
	const FOBB SmallQuery = Factory::FromAABB(FBox(FVector(45, -5, -5), FVector(55, 5, 5)), -1);
	Collection.FindAllOverlaps(SmallQuery, OverlapIndices);
	TestEqual(TEXT("Small query overlaps 1 box"), OverlapIndices.Num(), 1);

	return true;
}

//////////////////////////////////////////////////////////////////
// Segment Intersection Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionSegmentIntersectsAny,
	"PCGEx.Unit.Math.OBBCollection.SegmentQueries.IntersectsAny",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionSegmentIntersectsAny::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector(0, 0, 0)), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	// Segment through the box
	TestTrue(TEXT("Segment through box intersects"), Collection.SegmentIntersectsAny(FVector(-100, 0, 0), FVector(100, 0, 0)));

	// Segment missing the box
	TestFalse(TEXT("Segment missing box doesn't intersect"), Collection.SegmentIntersectsAny(FVector(-100, 200, 0), FVector(100, 200, 0)));

	return true;
}

//////////////////////////////////////////////////////////////////
// Bulk Operations Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionClassifyPoints,
	"PCGEx.Unit.Math.OBBCollection.BulkOps.ClassifyPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionClassifyPoints::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	TArray<FVector> Points = {
		FVector(0, 0, 0),    // Inside
		FVector(100, 0, 0),  // Outside
		FVector(25, 25, 25), // Inside
		FVector(-100, -100, -100) // Outside
	};

	TBitArray<> InsideMask;
	Collection.ClassifyPoints(Points, InsideMask);

	TestEqual(TEXT("Mask has correct size"), InsideMask.Num(), 4);
	TestTrue(TEXT("Point 0 is inside"), InsideMask[0]);
	TestFalse(TEXT("Point 1 is outside"), InsideMask[1]);
	TestTrue(TEXT("Point 2 is inside"), InsideMask[2]);
	TestFalse(TEXT("Point 3 is outside"), InsideMask[3]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionFilterInside,
	"PCGEx.Unit.Math.OBBCollection.BulkOps.FilterInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionFilterInside::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	TArray<FVector> Points = {
		FVector(0, 0, 0),    // Inside - index 0
		FVector(100, 0, 0),  // Outside
		FVector(25, 25, 25), // Inside - index 2
		FVector(-100, -100, -100) // Outside
	};

	TArray<int32> InsideIndices;
	Collection.FilterInside(Points, InsideIndices);

	TestEqual(TEXT("2 points inside"), InsideIndices.Num(), 2);
	TestTrue(TEXT("Contains index 0"), InsideIndices.Contains(0));
	TestTrue(TEXT("Contains index 2"), InsideIndices.Contains(2));

	return true;
}

//////////////////////////////////////////////////////////////////
// Bounds Query Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionLooseOverlaps,
	"PCGEx.Unit.Math.OBBCollection.BoundsQueries.LooseOverlaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionLooseOverlaps::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	// Box that overlaps world bounds
	const FBox OverlappingBox(FVector(40, -10, -10), FVector(60, 10, 10));
	TestTrue(TEXT("Loose overlaps returns true for overlapping box"), Collection.LooseOverlaps(OverlappingBox));

	// Box that doesn't overlap world bounds
	const FBox NonOverlappingBox(FVector(200, 200, 200), FVector(300, 300, 300));
	TestFalse(TEXT("Loose overlaps returns false for non-overlapping box"), Collection.LooseOverlaps(NonOverlappingBox));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionEncompasses,
	"PCGEx.Unit.Math.OBBCollection.BoundsQueries.Encompasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionEncompasses::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-100, -100, -100), FVector(100, 100, 100)), 0);
	Collection.BuildOctree();

	// Box fully inside
	const FBox InsideBox(FVector(-20, -20, -20), FVector(20, 20, 20));
	TestTrue(TEXT("Collection encompasses small box inside"), Collection.Encompasses(InsideBox));

	// Box partially outside
	const FBox PartialBox(FVector(80, -20, -20), FVector(120, 20, 20));
	TestFalse(TEXT("Collection does not encompass partially outside box"), Collection.Encompasses(PartialBox));

	// Box completely outside
	const FBox OutsideBox(FVector(200, 200, 200), FVector(250, 250, 250));
	TestFalse(TEXT("Collection does not encompass outside box"), Collection.Encompasses(OutsideBox));

	return true;
}

//////////////////////////////////////////////////////////////////
// Expansion Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCollectionIsPointInsideWithExpansion,
	"PCGEx.Unit.Math.OBBCollection.Expansion.PointInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCollectionIsPointInsideWithExpansion::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FCollection Collection;
	Collection.Add(FTransform(FVector::ZeroVector), FBox(FVector(-50, -50, -50), FVector(50, 50, 50)), 0);
	Collection.BuildOctree();

	// Point just outside
	const FVector JustOutside(55, 0, 0);

	// Without expansion, point is outside
	TestFalse(TEXT("Point outside without expansion"), Collection.IsPointInside(JustOutside));

	// With expansion of 10, point should be inside
	// NOTE: Must use ExpandedBox mode - Box mode ignores expansion parameter
	TestTrue(TEXT("Point inside with expansion"), Collection.IsPointInside(JustOutside, EPCGExBoxCheckMode::ExpandedBox, 10.0f));

	return true;
}
