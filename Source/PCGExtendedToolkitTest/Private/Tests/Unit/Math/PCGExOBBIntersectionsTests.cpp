// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExOBBIntersections Unit Tests
 *
 * Tests OBB-segment intersection utilities:
 * - FCut struct and cut type classification
 * - FIntersections container (add, sort, dedupe)
 * - SegmentBoxRaw - raw intersection computation
 * - ProcessSegment - cut processing
 * - SegmentIntersects - quick hit test
 *
 * These are pure geometry tests - no game world required.
 *
 * Test naming convention: PCGEx.Unit.OBB.Intersections.<Category>
 */

#include "Misc/AutomationTest.h"
#include "Math/OBB/PCGExOBB.h"
#include "Math/OBB/PCGExOBBIntersections.h"

using namespace PCGExMath::OBB;

// =============================================================================
// FCut Struct Tests
// =============================================================================

/**
 * Test FCut default construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCutDefaultTest,
	"PCGEx.Unit.OBB.Intersections.Cut.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCutDefaultTest::RunTest(const FString& Parameters)
{
	FCut Cut;

	TestTrue(TEXT("Default position is zero"),
	         Cut.Position.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Default normal is zero"),
	         Cut.Normal.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Default BoxIndex is -1"), Cut.BoxIndex, -1);
	TestEqual(TEXT("Default Idx is -1"), Cut.Idx, -1);
	TestEqual(TEXT("Default Type is Undefined"), Cut.Type, EPCGExCutType::Undefined);

	return true;
}

/**
 * Test FCut parameterized construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCutConstructorTest,
	"PCGEx.Unit.OBB.Intersections.Cut.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCutConstructorTest::RunTest(const FString& Parameters)
{
	FVector TestPos(100.0f, 200.0f, 300.0f);
	FVector TestNormal(0.0f, 1.0f, 0.0f);
	int32 BoxIdx = 5;
	int32 Idx = 10;

	FCut Cut(TestPos, TestNormal, BoxIdx, Idx, EPCGExCutType::Entry);

	TestTrue(TEXT("Position stored correctly"),
	         Cut.Position.Equals(TestPos, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Normal stored correctly"),
	         Cut.Normal.Equals(TestNormal, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("BoxIndex stored correctly"), Cut.BoxIndex, BoxIdx);
	TestEqual(TEXT("Idx stored correctly"), Cut.Idx, Idx);
	TestEqual(TEXT("Type stored correctly"), Cut.Type, EPCGExCutType::Entry);

	return true;
}

/**
 * Test FCut IsEntry/IsExit methods
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCutEntryExitTest,
	"PCGEx.Unit.OBB.Intersections.Cut.EntryExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCutEntryExitTest::RunTest(const FString& Parameters)
{
	FVector Pos = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;

	// Test Entry
	FCut EntryOnly(Pos, Normal, 0, 0, EPCGExCutType::Entry);
	TestTrue(TEXT("Entry is an entry cut"), EntryOnly.IsEntry());
	TestFalse(TEXT("Entry is not an exit cut"), EntryOnly.IsExit());

	// Test EntryNoExit
	FCut EntryNoExit(Pos, Normal, 0, 0, EPCGExCutType::EntryNoExit);
	TestTrue(TEXT("EntryNoExit is an entry cut"), EntryNoExit.IsEntry());
	TestFalse(TEXT("EntryNoExit is not an exit cut"), EntryNoExit.IsExit());

	// Test Exit
	FCut ExitOnly(Pos, Normal, 0, 0, EPCGExCutType::Exit);
	TestFalse(TEXT("Exit is not an entry cut"), ExitOnly.IsEntry());
	TestTrue(TEXT("Exit is an exit cut"), ExitOnly.IsExit());

	// Test ExitNoEntry
	FCut ExitNoEntry(Pos, Normal, 0, 0, EPCGExCutType::ExitNoEntry);
	TestFalse(TEXT("ExitNoEntry is not an entry cut"), ExitNoEntry.IsEntry());
	TestTrue(TEXT("ExitNoEntry is an exit cut"), ExitNoEntry.IsExit());

	// Test Undefined
	FCut Undefined(Pos, Normal, 0, 0, EPCGExCutType::Undefined);
	TestFalse(TEXT("Undefined is not an entry cut"), Undefined.IsEntry());
	TestFalse(TEXT("Undefined is not an exit cut"), Undefined.IsExit());

	return true;
}

// =============================================================================
// FIntersections Container Tests
// =============================================================================

/**
 * Test FIntersections default construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsDefaultTest,
	"PCGEx.Unit.OBB.Intersections.Container.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsDefaultTest::RunTest(const FString& Parameters)
{
	FIntersections Intersections;

	TestTrue(TEXT("Default is empty"), Intersections.IsEmpty());
	TestEqual(TEXT("Default Num is 0"), Intersections.Num(), 0);
	TestTrue(TEXT("Default Start is zero"),
	         Intersections.Start.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Default End is zero"),
	         Intersections.End.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FIntersections parameterized construction
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsConstructorTest,
	"PCGEx.Unit.OBB.Intersections.Container.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsConstructorTest::RunTest(const FString& Parameters)
{
	FVector Start(0, 0, 0);
	FVector End(100, 0, 0);

	FIntersections Intersections(Start, End);

	TestTrue(TEXT("Is empty initially"), Intersections.IsEmpty());
	TestTrue(TEXT("Start stored correctly"),
	         Intersections.Start.Equals(Start, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("End stored correctly"),
	         Intersections.End.Equals(End, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FIntersections Add and Reset
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsAddResetTest,
	"PCGEx.Unit.OBB.Intersections.Container.AddReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsAddResetTest::RunTest(const FString& Parameters)
{
	FIntersections Intersections(FVector::ZeroVector, FVector(100, 0, 0));

	// Add some cuts
	Intersections.Add(FVector(25, 0, 0), FVector(1, 0, 0), 0, 0, EPCGExCutType::Entry);
	Intersections.Add(FVector(75, 0, 0), FVector(-1, 0, 0), 0, 0, EPCGExCutType::Exit);

	TestEqual(TEXT("Has 2 cuts after adding"), Intersections.Num(), 2);
	TestFalse(TEXT("No longer empty"), Intersections.IsEmpty());

	// Verify cut data
	TestTrue(TEXT("First cut position correct"),
	         Intersections.Cuts[0].Position.Equals(FVector(25, 0, 0), KINDA_SMALL_NUMBER));
	TestEqual(TEXT("First cut type is Entry"), Intersections.Cuts[0].Type, EPCGExCutType::Entry);

	// Reset with new segment
	FVector NewStart(-50, 0, 0);
	FVector NewEnd(50, 0, 0);
	Intersections.Reset(NewStart, NewEnd);

	TestTrue(TEXT("Is empty after reset"), Intersections.IsEmpty());
	TestTrue(TEXT("New Start set correctly"),
	         Intersections.Start.Equals(NewStart, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("New End set correctly"),
	         Intersections.End.Equals(NewEnd, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FIntersections Sort (by distance from start)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsSortTest,
	"PCGEx.Unit.OBB.Intersections.Container.Sort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsSortTest::RunTest(const FString& Parameters)
{
	// Segment from origin along X
	FIntersections Intersections(FVector::ZeroVector, FVector(100, 0, 0));

	// Add cuts in non-sorted order
	Intersections.Add(FVector(75, 0, 0), FVector::ForwardVector, 2, 2, EPCGExCutType::Exit);
	Intersections.Add(FVector(25, 0, 0), FVector::ForwardVector, 0, 0, EPCGExCutType::Entry);
	Intersections.Add(FVector(50, 0, 0), FVector::ForwardVector, 1, 1, EPCGExCutType::Exit);

	// Verify unsorted order
	TestTrue(TEXT("Before sort: first cut at 75"),
	         Intersections.Cuts[0].Position.Equals(FVector(75, 0, 0), KINDA_SMALL_NUMBER));

	// Sort
	Intersections.Sort();

	// Verify sorted order (by distance from Start)
	TestTrue(TEXT("After sort: first cut at 25"),
	         Intersections.Cuts[0].Position.Equals(FVector(25, 0, 0), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("After sort: second cut at 50"),
	         Intersections.Cuts[1].Position.Equals(FVector(50, 0, 0), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("After sort: third cut at 75"),
	         Intersections.Cuts[2].Position.Equals(FVector(75, 0, 0), KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FIntersections SortAndDedupe
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsSortAndDedupeTest,
	"PCGEx.Unit.OBB.Intersections.Container.SortAndDedupe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsSortAndDedupeTest::RunTest(const FString& Parameters)
{
	FIntersections Intersections(FVector::ZeroVector, FVector(100, 0, 0));

	// Add cuts with duplicates at same position
	Intersections.Add(FVector(50, 0, 0), FVector::ForwardVector, 0, 0, EPCGExCutType::Entry);
	Intersections.Add(FVector(25, 0, 0), FVector::ForwardVector, 1, 1, EPCGExCutType::Entry);
	Intersections.Add(FVector(50, 0, 0), FVector::BackwardVector, 2, 2, EPCGExCutType::Exit); // Duplicate position
	Intersections.Add(FVector(75, 0, 0), FVector::ForwardVector, 3, 3, EPCGExCutType::Exit);

	TestEqual(TEXT("Has 4 cuts before dedupe"), Intersections.Num(), 4);

	Intersections.SortAndDedupe();

	// Should have 3 cuts (duplicate at 50 removed)
	TestEqual(TEXT("Has 3 cuts after dedupe"), Intersections.Num(), 3);

	// Verify order
	TestTrue(TEXT("First cut at 25"),
	         Intersections.Cuts[0].Position.Equals(FVector(25, 0, 0), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Second cut at 50"),
	         Intersections.Cuts[1].Position.Equals(FVector(50, 0, 0), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Third cut at 75"),
	         Intersections.Cuts[2].Position.Equals(FVector(75, 0, 0), KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test FIntersections GetBounds
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBIntersectionsGetBoundsTest,
	"PCGEx.Unit.OBB.Intersections.Container.GetBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBIntersectionsGetBoundsTest::RunTest(const FString& Parameters)
{
	FVector Start(0, 0, 0);
	FVector End(100, 50, 25);

	FIntersections Intersections(Start, End);
	FBoxCenterAndExtent Bounds = Intersections.GetBounds();

	// Expected center: (50, 25, 12.5)
	FVector ExpectedCenter = (Start + End) * 0.5;
	// FBoxCenterAndExtent uses FVector4, extract XYZ for comparison
	FVector BoundsCenter(Bounds.Center.X, Bounds.Center.Y, Bounds.Center.Z);
	TestTrue(TEXT("Bounds center is midpoint of segment"),
	         BoundsCenter.Equals(ExpectedCenter, 0.1f));

	// Extent should encompass both Start and End
	TestTrue(TEXT("Extent X >= half segment X length"),
	         Bounds.Extent.X >= 49.0f);
	TestTrue(TEXT("Extent Y >= half segment Y length"),
	         Bounds.Extent.Y >= 24.0f);

	return true;
}

// =============================================================================
// SegmentIntersects Quick Test
// =============================================================================

/**
 * Test SegmentIntersects for quick hit detection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentIntersectsTest,
	"PCGEx.Unit.OBB.Intersections.SegmentIntersects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentIntersectsTest::RunTest(const FString& Parameters)
{
	// Create axis-aligned box at origin with extents of 50
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	// Segment that passes through the box
	TestTrue(TEXT("Segment through box intersects"),
	         SegmentIntersects(Box, FVector(-100, 0, 0), FVector(100, 0, 0)));

	// Segment that misses completely
	TestFalse(TEXT("Segment above box doesn't intersect"),
	          SegmentIntersects(Box, FVector(-100, 0, 100), FVector(100, 0, 100)));

	// Segment that starts inside
	TestTrue(TEXT("Segment starting inside intersects"),
	         SegmentIntersects(Box, FVector(0, 0, 0), FVector(100, 0, 0)));

	// Segment that ends inside
	TestTrue(TEXT("Segment ending inside intersects"),
	         SegmentIntersects(Box, FVector(-100, 0, 0), FVector(0, 0, 0)));

	// Segment entirely inside
	TestTrue(TEXT("Segment entirely inside intersects"),
	         SegmentIntersects(Box, FVector(-25, 0, 0), FVector(25, 0, 0)));

	// Segment tangent to corner (borderline)
	TestTrue(TEXT("Segment touching corner intersects"),
	         SegmentIntersects(Box, FVector(50, 50, 0), FVector(100, 100, 0)));

	// Segment parallel to face, outside
	TestFalse(TEXT("Parallel segment outside doesn't intersect"),
	          SegmentIntersects(Box, FVector(-100, 100, 0), FVector(100, 100, 0)));

	return true;
}

/**
 * Test SegmentIntersects with rotated box
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentIntersectsRotatedTest,
	"PCGEx.Unit.OBB.Intersections.SegmentIntersects.Rotated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentIntersectsRotatedTest::RunTest(const FString& Parameters)
{
	// Create 45-degree rotated box
	FTransform RotatedTransform(FRotator(0, 45, 0), FVector::ZeroVector);
	FOBB Box = Factory::FromTransform(RotatedTransform, FVector(50.0f), 0);

	// Segment along X axis should still hit (goes through center)
	TestTrue(TEXT("X-axis segment hits rotated box"),
	         SegmentIntersects(Box, FVector(-100, 0, 0), FVector(100, 0, 0)));

	// Segment that misses rotated box
	// At 45 degrees, box Y-reach = 50*cos(45) + 50*sin(45) ≈ 70.7
	// A segment at Y=80 misses the rotated box (80 > 70.7)
	TestFalse(TEXT("Segment at Y=80 misses 45-degree rotated box"),
	          SegmentIntersects(Box, FVector(-100, 80, 0), FVector(100, 80, 0)));

	// Segment at Y=60 still hits (60 < 70.7)
	TestTrue(TEXT("Segment at Y=60 hits 45-degree rotated box"),
	         SegmentIntersects(Box, FVector(-100, 60, 0), FVector(100, 60, 0)));

	return true;
}

// =============================================================================
// SegmentBoxRaw Tests
// =============================================================================

/**
 * Test SegmentBoxRaw with no intersection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentBoxRawNoHitTest,
	"PCGEx.Unit.OBB.Intersections.SegmentBoxRaw.NoHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentBoxRawNoHitTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FVector Hit1, Hit2, Normal1, Normal2;
	bool bHit2Valid, bInverse;

	// Segment completely missing box
	bool bHit = SegmentBoxRaw(Box, FVector(-100, 100, 0), FVector(100, 100, 0),
	                          Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse);

	TestFalse(TEXT("Parallel segment above box reports no hit"), bHit);

	return true;
}

/**
 * Test SegmentBoxRaw with segment entirely inside (no surface intersection)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentBoxRawInsideTest,
	"PCGEx.Unit.OBB.Intersections.SegmentBoxRaw.Inside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentBoxRawInsideTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FVector Hit1, Hit2, Normal1, Normal2;
	bool bHit2Valid, bInverse;

	// Segment entirely inside box
	bool bHit = SegmentBoxRaw(Box, FVector(-25, 0, 0), FVector(25, 0, 0),
	                          Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse);

	TestFalse(TEXT("Segment entirely inside reports no surface hit"), bHit);

	return true;
}

/**
 * Test SegmentBoxRaw with pass-through (entry and exit)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentBoxRawPassThroughTest,
	"PCGEx.Unit.OBB.Intersections.SegmentBoxRaw.PassThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentBoxRawPassThroughTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FVector Hit1, Hit2, Normal1, Normal2;
	bool bHit2Valid, bInverse;

	// Segment passing through box along X axis
	bool bHit = SegmentBoxRaw(Box, FVector(-100, 0, 0), FVector(100, 0, 0),
	                          Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse);

	TestTrue(TEXT("Pass-through segment reports hit"), bHit);
	TestTrue(TEXT("Pass-through has two hits"), bHit2Valid);
	TestFalse(TEXT("Direction is not inverted"), bInverse);

	// First hit should be entry at -50
	TestTrue(TEXT("Entry hit at X=-50"),
	         FMath::IsNearlyEqual(Hit1.X, -50.0f, 1.0f));

	// Second hit should be exit at +50
	TestTrue(TEXT("Exit hit at X=+50"),
	         FMath::IsNearlyEqual(Hit2.X, 50.0f, 1.0f));

	// Normals should point outward
	TestTrue(TEXT("Entry normal points -X"),
	         FMath::IsNearlyEqual(Normal1.X, -1.0f, 0.1f));
	TestTrue(TEXT("Exit normal points +X"),
	         FMath::IsNearlyEqual(Normal2.X, 1.0f, 0.1f));

	return true;
}

/**
 * Test SegmentBoxRaw with start inside (exit only)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentBoxRawStartInsideTest,
	"PCGEx.Unit.OBB.Intersections.SegmentBoxRaw.StartInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentBoxRawStartInsideTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FVector Hit1, Hit2, Normal1, Normal2;
	bool bHit2Valid, bInverse;

	// Segment starting at center, exiting through +X face
	bool bHit = SegmentBoxRaw(Box, FVector(0, 0, 0), FVector(100, 0, 0),
	                          Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse);

	TestTrue(TEXT("Start-inside segment reports hit"), bHit);
	TestFalse(TEXT("Only one hit (exit)"), bHit2Valid);
	TestTrue(TEXT("Direction is inverted (traced from end)"), bInverse);

	// Hit should be at exit point ~50
	TestTrue(TEXT("Exit hit near X=50"),
	         FMath::IsNearlyEqual(Hit1.X, 50.0f, 1.0f));

	return true;
}

/**
 * Test SegmentBoxRaw with end inside (entry only)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSegmentBoxRawEndInsideTest,
	"PCGEx.Unit.OBB.Intersections.SegmentBoxRaw.EndInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSegmentBoxRawEndInsideTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);

	FVector Hit1, Hit2, Normal1, Normal2;
	bool bHit2Valid, bInverse;

	// Segment ending at center, entering through -X face
	bool bHit = SegmentBoxRaw(Box, FVector(-100, 0, 0), FVector(0, 0, 0),
	                          Hit1, Hit2, Normal1, Normal2, bHit2Valid, bInverse);

	TestTrue(TEXT("End-inside segment reports hit"), bHit);
	TestFalse(TEXT("Only one hit (entry)"), bHit2Valid);
	TestFalse(TEXT("Direction is not inverted"), bInverse);

	// Hit should be at entry point ~-50
	TestTrue(TEXT("Entry hit near X=-50"),
	         FMath::IsNearlyEqual(Hit1.X, -50.0f, 1.0f));

	return true;
}

// =============================================================================
// ProcessSegment Tests
// =============================================================================

/**
 * Test ProcessSegment with pass-through segment
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBProcessSegmentPassThroughTest,
	"PCGEx.Unit.OBB.Intersections.ProcessSegment.PassThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBProcessSegmentPassThroughTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 42);
	FIntersections IO(FVector(-100, 0, 0), FVector(100, 0, 0));

	bool bHit = ProcessSegment(Box, IO, 7);

	TestTrue(TEXT("ProcessSegment returns true for pass-through"), bHit);
	TestEqual(TEXT("Two cuts added (entry and exit)"), IO.Num(), 2);

	// Find entry and exit cuts
	const FCut* Entry = nullptr;
	const FCut* Exit = nullptr;
	for (const FCut& Cut : IO.Cuts)
	{
		if (Cut.Type == EPCGExCutType::Entry) Entry = &Cut;
		if (Cut.Type == EPCGExCutType::Exit) Exit = &Cut;
	}

	TestNotNull(TEXT("Has entry cut"), Entry);
	TestNotNull(TEXT("Has exit cut"), Exit);

	if (Entry)
	{
		TestEqual(TEXT("Entry BoxIndex is box index"), Entry->BoxIndex, 42);
		TestEqual(TEXT("Entry Idx is cloud index"), Entry->Idx, 7);
	}

	if (Exit)
	{
		TestEqual(TEXT("Exit BoxIndex is box index"), Exit->BoxIndex, 42);
		TestEqual(TEXT("Exit Idx is cloud index"), Exit->Idx, 7);
	}

	return true;
}

/**
 * Test ProcessSegment with start inside (exit only)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBProcessSegmentStartInsideTest,
	"PCGEx.Unit.OBB.Intersections.ProcessSegment.StartInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBProcessSegmentStartInsideTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	FIntersections IO(FVector(0, 0, 0), FVector(100, 0, 0));

	bool bHit = ProcessSegment(Box, IO);

	TestTrue(TEXT("ProcessSegment returns true for start-inside"), bHit);
	TestEqual(TEXT("One cut added (exit no entry)"), IO.Num(), 1);

	if (IO.Num() > 0)
	{
		TestEqual(TEXT("Cut type is ExitNoEntry"),
		          IO.Cuts[0].Type, EPCGExCutType::ExitNoEntry);
		TestTrue(TEXT("IsExit returns true"), IO.Cuts[0].IsExit());
		TestFalse(TEXT("IsEntry returns false"), IO.Cuts[0].IsEntry());
	}

	return true;
}

/**
 * Test ProcessSegment with end inside (entry only)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBProcessSegmentEndInsideTest,
	"PCGEx.Unit.OBB.Intersections.ProcessSegment.EndInside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBProcessSegmentEndInsideTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	FIntersections IO(FVector(-100, 0, 0), FVector(0, 0, 0));

	bool bHit = ProcessSegment(Box, IO);

	TestTrue(TEXT("ProcessSegment returns true for end-inside"), bHit);
	TestEqual(TEXT("One cut added (entry no exit)"), IO.Num(), 1);

	if (IO.Num() > 0)
	{
		TestEqual(TEXT("Cut type is EntryNoExit"),
		          IO.Cuts[0].Type, EPCGExCutType::EntryNoExit);
		TestTrue(TEXT("IsEntry returns true"), IO.Cuts[0].IsEntry());
		TestFalse(TEXT("IsExit returns false"), IO.Cuts[0].IsExit());
	}

	return true;
}

/**
 * Test ProcessSegment with no intersection
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBProcessSegmentNoHitTest,
	"PCGEx.Unit.OBB.Intersections.ProcessSegment.NoHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBProcessSegmentNoHitTest::RunTest(const FString& Parameters)
{
	FOBB Box = Factory::FromTransform(FTransform::Identity, FVector(50.0f), 0);
	FIntersections IO(FVector(-100, 100, 0), FVector(100, 100, 0));

	bool bHit = ProcessSegment(Box, IO);

	TestFalse(TEXT("ProcessSegment returns false for miss"), bHit);
	TestEqual(TEXT("No cuts added"), IO.Num(), 0);

	return true;
}

// =============================================================================
// EPCGExCutType Enum Tests
// =============================================================================

/**
 * Test EPCGExCutType enum values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBCutTypeEnumTest,
	"PCGEx.Unit.OBB.Intersections.CutType.EnumValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBCutTypeEnumTest::RunTest(const FString& Parameters)
{
	// Verify enum underlying values for stability
	TestEqual(TEXT("Undefined = 0"),
	          static_cast<uint8>(EPCGExCutType::Undefined), static_cast<uint8>(0));
	TestEqual(TEXT("Entry = 1"),
	          static_cast<uint8>(EPCGExCutType::Entry), static_cast<uint8>(1));
	TestEqual(TEXT("EntryNoExit = 2"),
	          static_cast<uint8>(EPCGExCutType::EntryNoExit), static_cast<uint8>(2));
	TestEqual(TEXT("Exit = 3"),
	          static_cast<uint8>(EPCGExCutType::Exit), static_cast<uint8>(3));
	TestEqual(TEXT("ExitNoEntry = 4"),
	          static_cast<uint8>(EPCGExCutType::ExitNoEntry), static_cast<uint8>(4));

	return true;
}
