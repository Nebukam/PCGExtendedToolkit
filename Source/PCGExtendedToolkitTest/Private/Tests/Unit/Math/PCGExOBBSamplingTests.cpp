// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Math/OBB/PCGExOBBSampling.h"
#include "Math/OBB/PCGExOBB.h"

//////////////////////////////////////////////////////////////////
// FSample Struct Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingFSampleDefaultState,
	"PCGEx.Unit.Math.OBBSampling.FSample.DefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingFSampleDefaultState::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	FSample Sample;

	TestEqual(TEXT("Distances default to zero"), Sample.Distances, FVector::ZeroVector);
	TestEqual(TEXT("UVW default to zero"), Sample.UVW, FVector::ZeroVector);
	TestEqual(TEXT("Weight default to zero"), Sample.Weight, 0.0);
	TestEqual(TEXT("BoxIndex default to -1"), Sample.BoxIndex, -1);
	TestFalse(TEXT("bIsInside default to false"), Sample.bIsInside);

	return true;
}

//////////////////////////////////////////////////////////////////
// Sample Function Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSamplePointAtCenter,
	"PCGEx.Unit.Math.OBBSampling.Sample.PointAtCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSamplePointAtCenter::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 0);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, FVector::ZeroVector, Sample);

	TestTrue(TEXT("Point at center is inside"), Sample.bIsInside);
	TestTrue(TEXT("Distances are near zero"), Sample.Distances.Equals(FVector::ZeroVector, 0.1f));
	TestTrue(TEXT("UVW is near zero"), Sample.UVW.Equals(FVector::ZeroVector, 0.01f));
	TestTrue(TEXT("Weight is 1 at center"), FMath::IsNearlyEqual(Sample.Weight, 1.0, 0.01));
	TestEqual(TEXT("BoxIndex matches"), Sample.BoxIndex, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSamplePointAtEdge,
	"PCGEx.Unit.Math.OBBSampling.Sample.PointAtEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSamplePointAtEdge::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 1);

	// Point at the positive X edge
	const FVector EdgePoint(50, 0, 0);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, EdgePoint, Sample);

	TestTrue(TEXT("Point at edge is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW.X is 1 at edge"), FMath::IsNearlyEqual(Sample.UVW.X, 1.0, 0.01));
	TestTrue(TEXT("UVW.Y is 0"), FMath::IsNearlyEqual(Sample.UVW.Y, 0.0, 0.01));
	TestTrue(TEXT("UVW.Z is 0"), FMath::IsNearlyEqual(Sample.UVW.Z, 0.0, 0.01));
	TestTrue(TEXT("Weight is 0 at edge"), FMath::IsNearlyEqual(Sample.Weight, 0.0, 0.01));
	TestEqual(TEXT("BoxIndex matches"), Sample.BoxIndex, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSamplePointOutside,
	"PCGEx.Unit.Math.OBBSampling.Sample.PointOutside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSamplePointOutside::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 2);

	// Point outside the box
	const FVector OutsidePoint(100, 0, 0);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, OutsidePoint, Sample);

	TestFalse(TEXT("Point outside is not inside"), Sample.bIsInside);
	TestTrue(TEXT("Weight is 0 when outside"), FMath::IsNearlyEqual(Sample.Weight, 0.0, 0.01));
	TestTrue(TEXT("UVW.X > 1 when outside on X"), Sample.UVW.X > 1.0);
	TestEqual(TEXT("BoxIndex matches"), Sample.BoxIndex, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSamplePointHalfway,
	"PCGEx.Unit.Math.OBBSampling.Sample.PointHalfway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSamplePointHalfway::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 3);

	// Point halfway from center to X edge
	const FVector HalfwayPoint(25, 0, 0);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, HalfwayPoint, Sample);

	TestTrue(TEXT("Point halfway is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW.X is 0.5 at halfway"), FMath::IsNearlyEqual(Sample.UVW.X, 0.5, 0.01));
	TestTrue(TEXT("Weight is 0.5 at halfway"), FMath::IsNearlyEqual(Sample.Weight, 0.5, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleRotatedBox,
	"PCGEx.Unit.Math.OBBSampling.Sample.RotatedBox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleRotatedBox::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Create a box rotated 45 degrees around Z
	FTransform Transform(FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0f)), FVector(100, 100, 0));
	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromTransform(Transform, LocalBox, 4);

	// Sample at the box center (which is at world 100, 100, 0)
	FSample Sample;
	PCGExMath::OBB::Sample(Box, FVector(100, 100, 0), Sample);

	TestTrue(TEXT("Point at rotated box center is inside"), Sample.bIsInside);
	TestTrue(TEXT("Local distances are near zero"), Sample.Distances.Equals(FVector::ZeroVector, 0.1f));
	TestTrue(TEXT("Weight is 1 at center"), FMath::IsNearlyEqual(Sample.Weight, 1.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleNonUniformExtents,
	"PCGEx.Unit.Math.OBBSampling.Sample.NonUniformExtents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleNonUniformExtents::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Box with different extents on each axis
	const FBox LocalBox(FVector(-100, -50, -25), FVector(100, 50, 25));
	const FOBB Box = Factory::FromAABB(LocalBox, 5);

	// Point at edge on each axis should have UVW = 1 for that axis
	FSample SampleX, SampleY, SampleZ;

	PCGExMath::OBB::Sample(Box, FVector(100, 0, 0), SampleX);
	PCGExMath::OBB::Sample(Box, FVector(0, 50, 0), SampleY);
	PCGExMath::OBB::Sample(Box, FVector(0, 0, 25), SampleZ);

	TestTrue(TEXT("UVW.X is 1 at X edge"), FMath::IsNearlyEqual(SampleX.UVW.X, 1.0, 0.01));
	TestTrue(TEXT("UVW.Y is 1 at Y edge"), FMath::IsNearlyEqual(SampleY.UVW.Y, 1.0, 0.01));
	TestTrue(TEXT("UVW.Z is 1 at Z edge"), FMath::IsNearlyEqual(SampleZ.UVW.Z, 1.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleNegativeUVW,
	"PCGEx.Unit.Math.OBBSampling.Sample.NegativeUVW",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleNegativeUVW::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 6);

	// Point in negative X direction
	const FVector NegativePoint(-25, 0, 0);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, NegativePoint, Sample);

	TestTrue(TEXT("Point is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW.X is -0.5 in negative direction"), FMath::IsNearlyEqual(Sample.UVW.X, -0.5, 0.01));

	return true;
}

//////////////////////////////////////////////////////////////////
// SampleFast Function Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleFastInside,
	"PCGEx.Unit.Math.OBBSampling.SampleFast.Inside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleFastInside::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 7);

	FSample Sample;
	SampleFast(Box, FVector(10, 10, 10), Sample);

	TestTrue(TEXT("Point is inside"), Sample.bIsInside);
	TestEqual(TEXT("BoxIndex matches"), Sample.BoxIndex, 7);
	// SampleFast doesn't compute UVW or Weight
	TestEqual(TEXT("UVW not computed (zero)"), Sample.UVW, FVector::ZeroVector);
	TestEqual(TEXT("Weight not computed (zero)"), Sample.Weight, 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleFastOutside,
	"PCGEx.Unit.Math.OBBSampling.SampleFast.Outside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleFastOutside::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 8);

	FSample Sample;
	SampleFast(Box, FVector(100, 0, 0), Sample);

	TestFalse(TEXT("Point is not inside"), Sample.bIsInside);
	TestEqual(TEXT("BoxIndex matches"), Sample.BoxIndex, 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleFastDistances,
	"PCGEx.Unit.Math.OBBSampling.SampleFast.Distances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleFastDistances::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 9);

	const FVector TestPoint(20, -30, 15);

	FSample Sample;
	SampleFast(Box, TestPoint, Sample);

	// Local distances should match the input point for an axis-aligned box at origin
	TestTrue(TEXT("Distances.X matches"), FMath::IsNearlyEqual(Sample.Distances.X, 20.0f, 0.1f));
	TestTrue(TEXT("Distances.Y matches"), FMath::IsNearlyEqual(Sample.Distances.Y, -30.0f, 0.1f));
	TestTrue(TEXT("Distances.Z matches"), FMath::IsNearlyEqual(Sample.Distances.Z, 15.0f, 0.1f));

	return true;
}

//////////////////////////////////////////////////////////////////
// SampleWithWeight Function Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleWithWeightCustomFunction,
	"PCGEx.Unit.Math.OBBSampling.SampleWithWeight.CustomFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleWithWeightCustomFunction::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 10);

	// Point at center
	FSample Sample;

	// Custom weight function that returns X component of UVW + 1
	auto CustomWeight = [](const FVector& UVW) -> double
	{
		return UVW.X + 1.0;
	};

	SampleWithWeight(Box, FVector::ZeroVector, Sample, CustomWeight);

	TestTrue(TEXT("Point is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW is computed"), Sample.UVW.Equals(FVector::ZeroVector, 0.01f));
	// At center, UVW.X = 0, so weight = 0 + 1 = 1
	TestTrue(TEXT("Custom weight is applied"), FMath::IsNearlyEqual(Sample.Weight, 1.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleWithWeightOffCenter,
	"PCGEx.Unit.Math.OBBSampling.SampleWithWeight.OffCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleWithWeightOffCenter::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 11);

	// Point at X = 25 (halfway to edge), UVW.X = 0.5
	FSample Sample;

	auto CustomWeight = [](const FVector& UVW) -> double
	{
		return UVW.X + 1.0;
	};

	SampleWithWeight(Box, FVector(25, 0, 0), Sample, CustomWeight);

	TestTrue(TEXT("Point is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW.X is 0.5"), FMath::IsNearlyEqual(Sample.UVW.X, 0.5, 0.01));
	// Weight = UVW.X + 1 = 0.5 + 1 = 1.5
	TestTrue(TEXT("Custom weight is 1.5"), FMath::IsNearlyEqual(Sample.Weight, 1.5, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleWithWeightOutside,
	"PCGEx.Unit.Math.OBBSampling.SampleWithWeight.Outside",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleWithWeightOutside::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 12);

	// Point outside
	FSample Sample;

	auto CustomWeight = [](const FVector& UVW) -> double
	{
		return 999.0; // Should not be called since point is outside
	};

	SampleWithWeight(Box, FVector(100, 0, 0), Sample, CustomWeight);

	TestFalse(TEXT("Point is not inside"), Sample.bIsInside);
	// When outside, weight is forced to 0, custom function not used
	TestTrue(TEXT("Weight is 0 when outside"), FMath::IsNearlyEqual(Sample.Weight, 0.0, 0.01));

	return true;
}

//////////////////////////////////////////////////////////////////
// Edge Cases
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleDegenerateExtent,
	"PCGEx.Unit.Math.OBBSampling.EdgeCases.DegenerateExtent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleDegenerateExtent::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Box with zero extent on Z (flat)
	const FBox LocalBox(FVector(-50, -50, 0), FVector(50, 50, 0));
	const FOBB Box = Factory::FromAABB(LocalBox, 13);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, FVector::ZeroVector, Sample);

	// UVW.Z should be 0 (safeguarded against divide by zero)
	TestTrue(TEXT("UVW.Z is 0 for degenerate extent"), FMath::IsNearlyEqual(Sample.UVW.Z, 0.0, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOBBSamplingSampleCorner,
	"PCGEx.Unit.Math.OBBSampling.EdgeCases.Corner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOBBSamplingSampleCorner::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	const FBox LocalBox(FVector(-50, -50, -50), FVector(50, 50, 50));
	const FOBB Box = Factory::FromAABB(LocalBox, 14);

	// Point at corner
	const FVector CornerPoint(50, 50, 50);

	FSample Sample;
	PCGExMath::OBB::Sample(Box, CornerPoint, Sample);

	TestTrue(TEXT("Point at corner is inside"), Sample.bIsInside);
	TestTrue(TEXT("UVW is (1, 1, 1) at corner"), Sample.UVW.Equals(FVector(1, 1, 1), 0.01f));
	// Weight is based on max axis ratio, which is 1, so weight = 1 - 1 = 0
	TestTrue(TEXT("Weight is 0 at corner"), FMath::IsNearlyEqual(Sample.Weight, 0.0, 0.01));

	return true;
}
