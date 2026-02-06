// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExTestHelpers.h"

#include "Data/PCGPointArrayData.h"
#include "Data/PCGExData.h"
#include "Elements/Layout/PCGExBinPacking3D.h"
#include "Elements/Layout/PCGExBinPacking.h"
#include "Helpers/PCGExPointArrayDataHelpers.h"

/**
 * PCGEx Layout Packing Unit Tests
 *
 * Tests the exported algorithms used by the bin packing nodes:
 * - Paper6 rotation symmetry reduction (BinPacking3D)
 * - RotateSize AABB computation
 * - FBP3DBin operations (support ratio, overlap, load bearing, contact score, EP)
 * - FBestFitBin rotation helpers
 * - Affinity key generation and scoring formulas (replicated from protected FProcessor)
 */

using namespace PCGExBinPacking3D;
using namespace PCGExTest;

//////////////////////////////////////////////////////////////////////////
// Test helpers
//////////////////////////////////////////////////////////////////////////

namespace
{
	// Create a single-point UPCGBasePointData with known bounds for bin construction
	UPCGBasePointData* CreateBinPointData(const FVector& Position, const FVector& HalfExtent, const FVector& Scale = FVector::OneVector)
	{
		UPCGBasePointData* Data = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(Data, 1, EPCGPointNativeProperties::All);
		Data->GetTransformValueRange()[0] = FTransform(FQuat::Identity, Position, Scale);
		Data->GetBoundsMinValueRange()[0] = -HalfExtent;
		Data->GetBoundsMaxValueRange()[0] = HalfExtent;
		return Data;
	}

	// Create a bin with known bounds: a 100x100x100 box from (-50,-50,-50) to (50,50,50), seed at min corner
	TSharedPtr<FBP3DBin> CreateTestBin(const FVector& HalfExtent = FVector(50), const FVector& SeedOffset = FVector(-1))
	{
		UPCGBasePointData* Data = CreateBinPointData(FVector::ZeroVector, HalfExtent);
		PCGExData::FConstPoint BinPoint(Data, 0);
		const FVector Seed = SeedOffset * HalfExtent;
		return MakeShared<FBP3DBin>(0, BinPoint, Seed);
	}

	// -- Affinity key (mirrors FProcessor::MakeAffinityKey, protected) --

	uint64 MakeAffinityKey(int32 A, int32 B)
	{
		if (A > B) { Swap(A, B); }
		return (static_cast<uint64>(static_cast<uint32>(A)) << 32) | static_cast<uint64>(static_cast<uint32>(B));
	}

	// -- Scoring formula (mirrors FProcessor::ComputeFinalScore, protected) --

	double ComputeBP3DScore(
		double WeightBinUsage, double WeightHeight, double WeightLoadBalance, double WeightContact,
		double BinUsageScore, double HeightScore, double LoadBalanceScore, double ContactScore)
	{
		return WeightBinUsage * BinUsageScore +
			WeightHeight * HeightScore +
			WeightLoadBalance * LoadBalanceScore +
			WeightContact * ContactScore;
	}
}

//////////////////////////////////////////////////////////////////////////
// Paper6 Rotation Tests (using real FBP3DRotationHelper)
//////////////////////////////////////////////////////////////////////////

#pragma region Paper6Rotations

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationCubeTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.Cube",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationCubeTest::RunTest(const FString& Parameters)
{
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(FVector(10, 10, 10), Rotations);
	TestEqual(TEXT("Cube produces 1 unique orientation"), Rotations.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationSquarePrismLWTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.SquarePrismLW",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationSquarePrismLWTest::RunTest(const FString& Parameters)
{
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(FVector(10, 10, 20), Rotations);
	TestEqual(TEXT("Square prism (L==W) produces 3 orientations"), Rotations.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationSquarePrismLHTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.SquarePrismLH",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationSquarePrismLHTest::RunTest(const FString& Parameters)
{
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(FVector(10, 20, 10), Rotations);
	TestEqual(TEXT("Square prism (L==H) produces 3 orientations"), Rotations.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationSquarePrismWHTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.SquarePrismWH",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationSquarePrismWHTest::RunTest(const FString& Parameters)
{
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(FVector(20, 10, 10), Rotations);
	TestEqual(TEXT("Square prism (W==H) produces 3 orientations"), Rotations.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationAllDifferentTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.AllDifferent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationAllDifferentTest::RunTest(const FString& Parameters)
{
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(FVector(10, 20, 30), Rotations);
	TestEqual(TEXT("All-different dimensions produce 6 orientations"), Rotations.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaper6RotationUniqueAABBsTest,
	"PCGEx.Unit.Layout.Packing.Paper6Rotations.UniqueAABBs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaper6RotationUniqueAABBsTest::RunTest(const FString& Parameters)
{
	const FVector Size(10, 20, 30);
	TArray<FRotator> Rotations;
	FBP3DRotationHelper::GetPaper6Rotations(Size, Rotations);

	TArray<FVector> Sizes;
	for (const FRotator& Rot : Rotations)
	{
		FVector RotatedSize = FBP3DRotationHelper::RotateSize(Size, Rot);

		TestTrue(FString::Printf(TEXT("Rotation (P=%.0f, Y=%.0f, R=%.0f) produces positive volume"), Rot.Pitch, Rot.Yaw, Rot.Roll),
			RotatedSize.X > KINDA_SMALL_NUMBER && RotatedSize.Y > KINDA_SMALL_NUMBER && RotatedSize.Z > KINDA_SMALL_NUMBER);

		// Volume conservation for orthogonal rotations
		const double OriginalVolume = Size.X * Size.Y * Size.Z;
		const double RotatedVolume = RotatedSize.X * RotatedSize.Y * RotatedSize.Z;
		PCGEX_TEST_NEARLY_EQUAL(RotatedVolume, OriginalVolume, 1.0, "Rotation preserves volume");

		// Store raw (unsorted) rotated size — each orientation should produce a unique axis assignment
		Sizes.Add(RotatedSize);
	}

	// All AABBs must be unique (different axis assignments)
	for (int32 i = 0; i < Sizes.Num(); i++)
	{
		for (int32 j = i + 1; j < Sizes.Num(); j++)
		{
			TestFalse(FString::Printf(TEXT("Orientations %d and %d produce different AABBs"), i, j),
				Sizes[i].Equals(Sizes[j], 0.5));
		}
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// RotateSize Tests (using real FBP3DRotationHelper)
//////////////////////////////////////////////////////////////////////////

#pragma region RotateSize

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRotateSizeIdentityTest,
	"PCGEx.Unit.Layout.Packing.RotateSize.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExRotateSizeIdentityTest::RunTest(const FString& Parameters)
{
	const FVector Size(10, 20, 30);
	const FVector Result = FBP3DRotationHelper::RotateSize(Size, FRotator::ZeroRotator);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Result, Size, 0.1, "Identity rotation preserves size");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRotateSizeYaw90Test,
	"PCGEx.Unit.Layout.Packing.RotateSize.Yaw90",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExRotateSizeYaw90Test::RunTest(const FString& Parameters)
{
	const FVector Size(10, 20, 30);
	const FVector Result = FBP3DRotationHelper::RotateSize(Size, FRotator(0, 90, 0));
	const FVector Expected(20, 10, 30);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Result, Expected, 0.1, "Yaw 90 swaps X and Y");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRotateSizePitch90Test,
	"PCGEx.Unit.Layout.Packing.RotateSize.Pitch90",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExRotateSizePitch90Test::RunTest(const FString& Parameters)
{
	const FVector Size(10, 20, 30);
	const FVector Result = FBP3DRotationHelper::RotateSize(Size, FRotator(90, 0, 0));
	const FVector Expected(30, 20, 10);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Result, Expected, 0.1, "Pitch 90 swaps X and Z");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRotateSizeRoll90Test,
	"PCGEx.Unit.Layout.Packing.RotateSize.Roll90",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExRotateSizeRoll90Test::RunTest(const FString& Parameters)
{
	const FVector Size(10, 20, 30);
	const FVector Result = FBP3DRotationHelper::RotateSize(Size, FRotator(0, 0, 90));
	const FVector Expected(10, 30, 20);
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(Result, Expected, 0.1, "Roll 90 swaps Y and Z");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExRotateSizeVolumeConservationTest,
	"PCGEx.Unit.Layout.Packing.RotateSize.VolumeConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExRotateSizeVolumeConservationTest::RunTest(const FString& Parameters)
{
	const FVector Size(15, 25, 35);
	const double OriginalVolume = Size.X * Size.Y * Size.Z;

	const TArray<FRotator> TestRotations = {
		FRotator(0, 45, 0),
		FRotator(30, 60, 90),
		FRotator(90, 90, 0),
		FRotator(0, 90, 90),
	};

	for (const FRotator& Rot : TestRotations)
	{
		const FVector Rotated = FBP3DRotationHelper::RotateSize(Size, Rot);

		const bool bOrthogonal = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rot.Pitch), 90.0), 1.0) &&
			FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rot.Yaw), 90.0), 1.0) &&
			FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Rot.Roll), 90.0), 1.0);

		const double RotatedVolume = Rotated.X * Rotated.Y * Rotated.Z;

		if (bOrthogonal)
		{
			PCGEX_TEST_NEARLY_EQUAL(RotatedVolume, OriginalVolume, 1.0, "Orthogonal rotation conserves volume");
		}
		else
		{
			TestTrue(TEXT("Non-orthogonal AABB volume >= original"), RotatedVolume >= OriginalVolume - 1.0);
		}

		TestTrue(TEXT("Rotated X > 0"), Rotated.X > 0);
		TestTrue(TEXT("Rotated Y > 0"), Rotated.Y > 0);
		TestTrue(TEXT("Rotated Z > 0"), Rotated.Z > 0);
	}

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FBP3DBin Support Ratio Tests (using real FBP3DBin)
//////////////////////////////////////////////////////////////////////////

#pragma region SupportRatio

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSupportRatioFloorTest,
	"PCGEx.Unit.Layout.Packing.SupportRatio.FloorItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSupportRatioFloorTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	// Item sitting on the floor (Z min matches bin Z min)
	const FBox FloorItem(FVector(-10, -10, -50), FVector(10, 10, -30));
	const double Ratio = Bin->ComputeSupportRatio(FloorItem);
	PCGEX_TEST_NEARLY_EQUAL(Ratio, 1.0, KINDA_SMALL_NUMBER, "Floor item has full support");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSupportRatioFullySupportedTest,
	"PCGEx.Unit.Layout.Packing.SupportRatio.FullySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSupportRatioFullySupportedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	// Manually add a base item
	FBP3DItem Base;
	Base.Index = 0;
	Base.Box = FBox(FVector(-50, -50, -50), FVector(-30, -30, -30));
	Base.PaddedBox = Base.Box;
	Bin->Items.Add(Base);

	// Smaller item sitting exactly on top, fully within support footprint
	const FBox TopItem(FVector(-45, -45, -30), FVector(-35, -35, -10));
	const double Ratio = Bin->ComputeSupportRatio(TopItem);
	PCGEX_TEST_NEARLY_EQUAL(Ratio, 1.0, KINDA_SMALL_NUMBER, "Item fully within support footprint");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSupportRatioPartiallySupportedTest,
	"PCGEx.Unit.Layout.Packing.SupportRatio.PartiallySupported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSupportRatioPartiallySupportedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Base;
	Base.Index = 0;
	Base.Box = FBox(FVector(0, 0, -50), FVector(20, 20, 0));
	Base.PaddedBox = Base.Box;
	Bin->Items.Add(Base);

	// Top item half-overhanging: 10x20 overlap on 20x20 base = 50%
	const FBox TopItem(FVector(10, 0, 0), FVector(30, 20, 20));
	const double Ratio = Bin->ComputeSupportRatio(TopItem);
	PCGEX_TEST_NEARLY_EQUAL(Ratio, 0.5, KINDA_SMALL_NUMBER, "Half-overhanging item has 50% support");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSupportRatioFloatingTest,
	"PCGEx.Unit.Layout.Packing.SupportRatio.Floating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSupportRatioFloatingTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	// No items, item not on floor
	const FBox FloatingItem(FVector(-10, -10, 0), FVector(10, 10, 20));
	const double Ratio = Bin->ComputeSupportRatio(FloatingItem);
	PCGEX_TEST_NEARLY_EQUAL(Ratio, 0.0, KINDA_SMALL_NUMBER, "Floating item has no support");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSupportRatioMultipleSupportersTest,
	"PCGEx.Unit.Layout.Packing.SupportRatio.MultipleSupporters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExSupportRatioMultipleSupportersTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Left;
	Left.Index = 0;
	Left.Box = FBox(FVector(0, 0, -50), FVector(10, 20, 0));
	Left.PaddedBox = Left.Box;
	Bin->Items.Add(Left);

	FBP3DItem Right;
	Right.Index = 1;
	Right.Box = FBox(FVector(10, 0, -50), FVector(20, 20, 0));
	Right.PaddedBox = Right.Box;
	Bin->Items.Add(Right);

	// Item bridging both: left 10*20 + right 10*20 = 400, base 20*20 = 400
	const FBox BridgeItem(FVector(0, 0, 0), FVector(20, 20, 20));
	const double Ratio = Bin->ComputeSupportRatio(BridgeItem);
	PCGEX_TEST_NEARLY_EQUAL(Ratio, 1.0, KINDA_SMALL_NUMBER, "Item bridging two supporters is fully supported");
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FBP3DBin Overlap Detection Tests
//////////////////////////////////////////////////////////////////////////

#pragma region Overlap

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOverlapNoneTest,
	"PCGEx.Unit.Layout.Packing.Overlap.NoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExOverlapNoneTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Item;
	Item.Index = 0;
	Item.PaddedBox = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
	Bin->Items.Add(Item);

	const FBox TestBox(FVector(20, 20, 20), FVector(30, 30, 30));
	TestFalse(TEXT("Separate boxes don't overlap"), Bin->HasOverlap(TestBox));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOverlapTrueTest,
	"PCGEx.Unit.Layout.Packing.Overlap.Overlapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExOverlapTrueTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Item;
	Item.Index = 0;
	Item.PaddedBox = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
	Bin->Items.Add(Item);

	const FBox TestBox(FVector(5, 5, 5), FVector(15, 15, 15));
	TestTrue(TEXT("Penetrating boxes overlap"), Bin->HasOverlap(TestBox));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOverlapTouchingFacesTest,
	"PCGEx.Unit.Layout.Packing.Overlap.TouchingFaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExOverlapTouchingFacesTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Item;
	Item.Index = 0;
	Item.PaddedBox = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
	Bin->Items.Add(Item);

	const FBox TestBox(FVector(10, 0, 0), FVector(20, 10, 10));
	TestFalse(TEXT("Touching faces don't count as overlap"), Bin->HasOverlap(TestBox));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOverlapEmptyBinTest,
	"PCGEx.Unit.Layout.Packing.Overlap.EmptyBin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExOverlapEmptyBinTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();
	const FBox TestBox(FVector(0, 0, 0), FVector(10, 10, 10));
	TestFalse(TEXT("No overlap in empty bin"), Bin->HasOverlap(TestBox));
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FBP3DBin Load Bearing Tests
//////////////////////////////////////////////////////////////////////////

#pragma region LoadBearing

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLoadBearingEmptyBinTest,
	"PCGEx.Unit.Layout.Packing.LoadBearing.EmptyBin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLoadBearingEmptyBinTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();
	FBP3DPlacementCandidate Candidate;
	Candidate.PlacementMin = FVector(0, 0, 0);
	Candidate.RotatedSize = FVector(10, 10, 10);
	TestTrue(TEXT("Any item passes load bearing in empty bin"), Bin->CheckLoadBearing(Candidate, 100.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLoadBearingLighterOnTopTest,
	"PCGEx.Unit.Layout.Packing.LoadBearing.LighterOnTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLoadBearingLighterOnTopTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Heavy;
	Heavy.Index = 0;
	Heavy.Box = FBox(FVector(0, 0, 0), FVector(20, 20, 10));
	Heavy.PaddedBox = Heavy.Box;
	Heavy.Weight = 10.0;
	Bin->Items.Add(Heavy);

	FBP3DPlacementCandidate Candidate;
	Candidate.PlacementMin = FVector(0, 0, 10);
	Candidate.RotatedSize = FVector(20, 20, 10);

	TestTrue(TEXT("Lighter item on top passes (5 <= 1.0*10)"), Bin->CheckLoadBearing(Candidate, 5.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLoadBearingHeavierOnTopTest,
	"PCGEx.Unit.Layout.Packing.LoadBearing.HeavierOnTop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLoadBearingHeavierOnTopTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Light;
	Light.Index = 0;
	Light.Box = FBox(FVector(0, 0, 0), FVector(20, 20, 10));
	Light.PaddedBox = Light.Box;
	Light.Weight = 5.0;
	Bin->Items.Add(Light);

	FBP3DPlacementCandidate Candidate;
	Candidate.PlacementMin = FVector(0, 0, 10);
	Candidate.RotatedSize = FVector(20, 20, 10);

	TestFalse(TEXT("Heavier item on top fails (10 > 1.0*5)"), Bin->CheckLoadBearing(Candidate, 10.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLoadBearingBesideNotAboveTest,
	"PCGEx.Unit.Layout.Packing.LoadBearing.BesideNotAbove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLoadBearingBesideNotAboveTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Light;
	Light.Index = 0;
	Light.Box = FBox(FVector(0, 0, 0), FVector(10, 10, 10));
	Light.PaddedBox = Light.Box;
	Light.Weight = 1.0;
	Bin->Items.Add(Light);

	FBP3DPlacementCandidate Candidate;
	Candidate.PlacementMin = FVector(20, 0, 0);
	Candidate.RotatedSize = FVector(10, 10, 10);

	TestTrue(TEXT("Heavy item beside (not above) passes"), Bin->CheckLoadBearing(Candidate, 100.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExLoadBearingCustomThresholdTest,
	"PCGEx.Unit.Layout.Packing.LoadBearing.CustomThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExLoadBearingCustomThresholdTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Base;
	Base.Index = 0;
	Base.Box = FBox(FVector(0, 0, 0), FVector(20, 20, 10));
	Base.PaddedBox = Base.Box;
	Base.Weight = 10.0;
	Bin->Items.Add(Base);

	FBP3DPlacementCandidate Candidate;
	Candidate.PlacementMin = FVector(0, 0, 10);
	Candidate.RotatedSize = FVector(20, 20, 10);

	TestTrue(TEXT("Weight 5 passes with threshold 0.5 (5 <= 0.5*10)"), Bin->CheckLoadBearing(Candidate, 5.0, 0.5));
	TestFalse(TEXT("Weight 6 fails with threshold 0.5 (6 > 0.5*10)"), Bin->CheckLoadBearing(Candidate, 6.0, 0.5));
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FBP3DBin Contact Score Tests
//////////////////////////////////////////////////////////////////////////

#pragma region ContactScore

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExContactScoreCornerItemTest,
	"PCGEx.Unit.Layout.Packing.ContactScore.CornerItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExContactScoreCornerItemTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	// Item tucked into the Min corner: touches 3 bin walls
	const FBox CornerItem(FVector(-50, -50, -50), FVector(-30, -30, -30));
	const double Score = Bin->ComputeContactScore(CornerItem);

	// 3 contacts out of 6 max -> score = 1.0 - 3/6 = 0.5
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.5, KINDA_SMALL_NUMBER, "Corner item touching 3 walls has score 0.5");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExContactScoreFloatingItemTest,
	"PCGEx.Unit.Layout.Packing.ContactScore.FloatingItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExContactScoreFloatingItemTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	const FBox CenterItem(FVector(-5, -5, -5), FVector(5, 5, 5));
	const double Score = Bin->ComputeContactScore(CenterItem);
	PCGEX_TEST_NEARLY_EQUAL(Score, 1.0, KINDA_SMALL_NUMBER, "Centered item touching no walls has score 1.0 (worst)");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExContactScoreWithNeighborTest,
	"PCGEx.Unit.Layout.Packing.ContactScore.WithNeighbor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExContactScoreWithNeighborTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DItem Existing;
	Existing.Index = 0;
	Existing.Box = FBox(FVector(-50, -50, -50), FVector(-30, -30, -30));
	Existing.PaddedBox = Existing.Box;
	Bin->Items.Add(Existing);

	// New item touching existing item's +X face + 2 bin walls (Y min, Z min)
	const FBox Neighbor(FVector(-30, -50, -50), FVector(-10, -30, -30));
	const double Score = Bin->ComputeContactScore(Neighbor);

	// 3 contacts: Y min wall, Z min wall, existing item's X face = 1.0 - 3/6 = 0.5
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.5, KINDA_SMALL_NUMBER, "Item with 2 wall + 1 neighbor contacts has score 0.5");
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// FBP3DBin Construction & EP Tests
//////////////////////////////////////////////////////////////////////////

#pragma region BinConstruction

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBinConstructionTest,
	"PCGEx.Unit.Layout.Packing.Bin.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBinConstructionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	TestEqual(TEXT("Bin index is 0"), Bin->BinIndex, 0);
	TestTrue(TEXT("No items initially"), Bin->Items.IsEmpty());
	PCGEX_TEST_NEARLY_EQUAL(Bin->GetFillRatio(), 0.0, KINDA_SMALL_NUMBER, "Fill ratio starts at 0");
	TestEqual(TEXT("Initial EP count is 1"), Bin->GetEPCount(), 1);
	PCGEX_TEST_NEARLY_EQUAL(Bin->CurrentWeight, 0.0, KINDA_SMALL_NUMBER, "Weight starts at 0");

	// Bounds should be 100x100x100 centered at origin
	const FVector BoundsSize = Bin->Bounds.GetSize();
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(BoundsSize, FVector(100, 100, 100), 1.0, "Bin is 100x100x100");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBinEvaluatePlacementTest,
	"PCGEx.Unit.Layout.Packing.Bin.EvaluatePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBinEvaluatePlacementTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	// Small item should fit (zero padding)
	FBP3DPlacementCandidate Candidate;
	const bool bFits = Bin->EvaluatePlacement(FVector(20, 20, 20), FVector::ZeroVector, 0, FRotator::ZeroRotator, Candidate);
	TestTrue(TEXT("20x20x20 item fits in 100x100x100 bin"), bFits);
	TestTrue(TEXT("Candidate is valid"), Candidate.IsValid());
	TestEqual(TEXT("Candidate bin index matches"), Candidate.BinIndex, 0);

	// Item too large should not fit
	FBP3DPlacementCandidate LargeCandidate;
	const bool bLargeFits = Bin->EvaluatePlacement(FVector(200, 200, 200), FVector::ZeroVector, 0, FRotator::ZeroRotator, LargeCandidate);
	TestFalse(TEXT("200x200x200 item doesn't fit"), bLargeFits);

	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Affinity Key Tests (replicated from protected FProcessor)
//////////////////////////////////////////////////////////////////////////

#pragma region AffinityKey

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAffinityKeySymmetryTest,
	"PCGEx.Unit.Layout.Packing.AffinityKey.Symmetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAffinityKeySymmetryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Key(1,2) == Key(2,1)"), MakeAffinityKey(1, 2), MakeAffinityKey(2, 1));
	TestEqual(TEXT("Key(0,100) == Key(100,0)"), MakeAffinityKey(0, 100), MakeAffinityKey(100, 0));
	TestEqual(TEXT("Key(5,5) == Key(5,5)"), MakeAffinityKey(5, 5), MakeAffinityKey(5, 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAffinityKeyUniquenessTest,
	"PCGEx.Unit.Layout.Packing.AffinityKey.Uniqueness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAffinityKeyUniquenessTest::RunTest(const FString& Parameters)
{
	TestNotEqual(TEXT("Key(1,2) != Key(1,3)"), MakeAffinityKey(1, 2), MakeAffinityKey(1, 3));
	TestNotEqual(TEXT("Key(1,2) != Key(2,3)"), MakeAffinityKey(1, 2), MakeAffinityKey(2, 3));
	TestNotEqual(TEXT("Key(0,1) != Key(0,2)"), MakeAffinityKey(0, 1), MakeAffinityKey(0, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAffinityKeyNegativeValuesTest,
	"PCGEx.Unit.Layout.Packing.AffinityKey.NegativeValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAffinityKeyNegativeValuesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Key(-1,2) == Key(2,-1)"), MakeAffinityKey(-1, 2), MakeAffinityKey(2, -1));
	TestNotEqual(TEXT("Key(-1,2) != Key(1,2)"), MakeAffinityKey(-1, 2), MakeAffinityKey(1, 2));
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Scoring Formula Tests (replicated from protected FProcessor)
//////////////////////////////////////////////////////////////////////////

#pragma region Scoring

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DScoringZeroWeightsTest,
	"PCGEx.Unit.Layout.Packing.Scoring.ZeroWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DScoringZeroWeightsTest::RunTest(const FString& Parameters)
{
	const double Score = ComputeBP3DScore(0, 0, 0, 0, 0.5, 0.5, 0.5, 0.5);
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.0, KINDA_SMALL_NUMBER, "All-zero weights produce zero score");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DScoringUniformWeightsTest,
	"PCGEx.Unit.Layout.Packing.Scoring.UniformWeights",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DScoringUniformWeightsTest::RunTest(const FString& Parameters)
{
	const double Score = ComputeBP3DScore(0.25, 0.25, 0.25, 0.25, 0.8, 0.8, 0.8, 0.8);
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.8, KINDA_SMALL_NUMBER, "Uniform weights with equal objectives = objective value");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DScoringSingleObjectiveTest,
	"PCGEx.Unit.Layout.Packing.Scoring.SingleObjective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DScoringSingleObjectiveTest::RunTest(const FString& Parameters)
{
	const double Score = ComputeBP3DScore(0.0, 1.0, 0.0, 0.0, 0.9, 0.3, 0.9, 0.9);
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.3, KINDA_SMALL_NUMBER, "Single active objective dominates");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DScoringWeightedCombinationTest,
	"PCGEx.Unit.Layout.Packing.Scoring.WeightedCombination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DScoringWeightedCombinationTest::RunTest(const FString& Parameters)
{
	// 0.3*0.4 + 0.3*0.6 + 0.2*0.8 + 0.2*0.2 = 0.12 + 0.18 + 0.16 + 0.04 = 0.5
	const double Score = ComputeBP3DScore(0.3, 0.3, 0.2, 0.2, 0.4, 0.6, 0.8, 0.2);
	PCGEX_TEST_NEARLY_EQUAL(Score, 0.5, KINDA_SMALL_NUMBER, "Weighted combination matches expected value");
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Enum and Struct Default Tests
//////////////////////////////////////////////////////////////////////////

#pragma region EnumsAndStructs

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DRotationModeEnumTest,
	"PCGEx.Unit.Layout.Packing.Enums.RotationMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DRotationModeEnumTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("None = 0"), static_cast<uint8>(EPCGExBP3DRotationMode::None), 0);
	TestEqual(TEXT("CardinalOnly = 1"), static_cast<uint8>(EPCGExBP3DRotationMode::CardinalOnly), 1);
	TestEqual(TEXT("Paper6 = 2"), static_cast<uint8>(EPCGExBP3DRotationMode::Paper6), 2);
	TestEqual(TEXT("AllOrthogonal = 3"), static_cast<uint8>(EPCGExBP3DRotationMode::AllOrthogonal), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DItemDefaultsTest,
	"PCGEx.Unit.Layout.Packing.Structs.BP3DItemDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DItemDefaultsTest::RunTest(const FString& Parameters)
{
	FBP3DItem Item;
	TestEqual(TEXT("Default index is -1"), Item.Index, -1);
	TestTrue(TEXT("Default padding is zero"), Item.Padding.IsNearlyZero());
	TestTrue(TEXT("Default rotation is zero"), Item.Rotation.IsNearlyZero());
	PCGEX_TEST_NEARLY_EQUAL(Item.Weight, 0.0, KINDA_SMALL_NUMBER, "Default weight is 0");
	TestEqual(TEXT("Default category is -1"), Item.Category, -1);
	PCGEX_TEST_NEARLY_EQUAL(Item.LoadBearingThreshold, 1.0, KINDA_SMALL_NUMBER, "Default load bearing threshold is 1.0");
	PCGEX_TEST_NEARLY_EQUAL(Item.MinSupportRatio, 0.0, KINDA_SMALL_NUMBER, "Default min support ratio is 0.0");
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExBP3DPlacementCandidateValidityTest,
	"PCGEx.Unit.Layout.Packing.Structs.PlacementCandidateValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExBP3DPlacementCandidateValidityTest::RunTest(const FString& Parameters)
{
	FBP3DPlacementCandidate Candidate;

	TestFalse(TEXT("Default is invalid"), Candidate.IsValid());

	Candidate.BinIndex = 0;
	Candidate.EPIndex = -1;
	TestFalse(TEXT("Only bin set is invalid"), Candidate.IsValid());

	Candidate.BinIndex = -1;
	Candidate.EPIndex = 0;
	TestFalse(TEXT("Only EP set is invalid"), Candidate.IsValid());

	Candidate.BinIndex = 0;
	Candidate.EPIndex = 0;
	TestTrue(TEXT("Both set is valid"), Candidate.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExAffinityRuleDefaultsTest,
	"PCGEx.Unit.Layout.Packing.Structs.AffinityRuleDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExAffinityRuleDefaultsTest::RunTest(const FString& Parameters)
{
	FPCGExBP3DAffinityRule Rule;
	TestEqual(TEXT("Default type is Negative"), Rule.Type, EPCGExBP3DAffinityType::Negative);
	TestEqual(TEXT("Default CategoryA is 0"), Rule.CategoryA, 0);
	TestEqual(TEXT("Default CategoryB is 1"), Rule.CategoryB, 1);
	return true;
}

#pragma endregion

//////////////////////////////////////////////////////////////////////////
// Padding Behavior Tests
//////////////////////////////////////////////////////////////////////////

#pragma region Padding

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaddingReducesFittableItemsTest,
	"PCGEx.Unit.Layout.Packing.Padding.ReducesFittableItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaddingReducesFittableItemsTest::RunTest(const FString& Parameters)
{
	// 100x100x100 bin, 45x45x45 items
	// Without padding: two items side by side = 90 < 100, fits
	// With padding 5: padded size = 55, two items = 110 > 100, second won't fit
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	const FVector ItemSize(45, 45, 45);
	const FVector Padding(5, 5, 5);

	// First item with padding should fit
	FBP3DPlacementCandidate First;
	const bool bFirstFits = Bin->EvaluatePlacement(ItemSize, Padding, 0, FRotator::ZeroRotator, First);
	TestTrue(TEXT("First 45^3 item with padding 5 fits"), bFirstFits);

	// Commit it
	FBP3DItem FirstItem;
	FirstItem.Index = 0;
	FirstItem.Padding = Padding;
	Bin->CommitPlacement(First, FirstItem);

	// Verify actual box is inset from padded box
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(FirstItem.Box.GetSize(), ItemSize, 0.1, "Actual box matches item size");
	const FVector ExpectedPaddedSize = ItemSize + Padding * 2.0;
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(FirstItem.PaddedBox.GetSize(), ExpectedPaddedSize, 0.1, "Padded box is expanded by padding");

	// Without padding, a second 45^3 item should fit (45+45=90 < 100)
	FBP3DPlacementCandidate SecondNoPad;
	bool bAnyFitNoPad = false;
	for (int32 EP = 0; EP < Bin->GetEPCount(); EP++)
	{
		if (Bin->EvaluatePlacement(ItemSize, FVector::ZeroVector, EP, FRotator::ZeroRotator, SecondNoPad))
		{
			bAnyFitNoPad = true;
			break;
		}
	}
	TestTrue(TEXT("Second 45^3 item WITHOUT padding can fit"), bAnyFitNoPad);

	// With padding, a second 45^3 item should NOT fit (55+55=110 > 100)
	FBP3DPlacementCandidate SecondPadded;
	bool bAnyFitPadded = false;
	for (int32 EP = 0; EP < Bin->GetEPCount(); EP++)
	{
		if (Bin->EvaluatePlacement(ItemSize, Padding, EP, FRotator::ZeroRotator, SecondPadded))
		{
			bAnyFitPadded = true;
			break;
		}
	}
	TestFalse(TEXT("Second 45^3 item WITH padding 5 cannot fit (55+55=110>100)"), bAnyFitPadded);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaddingZeroMatchesNoPaddingTest,
	"PCGEx.Unit.Layout.Packing.Padding.ZeroMatchesNoPadding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaddingZeroMatchesNoPaddingTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DPlacementCandidate CandidateZero;
	const bool bFitsZero = Bin->EvaluatePlacement(FVector(30, 30, 30), FVector::ZeroVector, 0, FRotator::ZeroRotator, CandidateZero);
	TestTrue(TEXT("Fits with zero padding"), bFitsZero);

	// PlacementMin should be identical with zero padding
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(CandidateZero.EffectivePadding, FVector::ZeroVector, KINDA_SMALL_NUMBER, "Effective padding is zero");

	// RotatedSize should be the actual item size
	PCGEX_TEST_VECTOR_NEARLY_EQUAL(CandidateZero.RotatedSize, FVector(30, 30, 30), 0.1, "RotatedSize matches item size");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaddingCreatesGapsTest,
	"PCGEx.Unit.Layout.Packing.Padding.CreatesGaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaddingCreatesGapsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	const FVector ItemSize(20, 20, 20);
	const FVector Padding(5, 5, 5);

	// Place first item
	FBP3DPlacementCandidate First;
	Bin->EvaluatePlacement(ItemSize, Padding, 0, FRotator::ZeroRotator, First);

	FBP3DItem FirstItem;
	FirstItem.Index = 0;
	FirstItem.Padding = Padding;
	Bin->CommitPlacement(First, FirstItem);

	// Place second item at a different EP
	FBP3DPlacementCandidate Second;
	bool bPlaced = false;
	for (int32 EP = 0; EP < Bin->GetEPCount(); EP++)
	{
		if (Bin->EvaluatePlacement(ItemSize, Padding, EP, FRotator::ZeroRotator, Second))
		{
			bPlaced = true;
			break;
		}
	}

	if (bPlaced)
	{
		// The actual boxes should have a gap of at least 2*padding between them
		const FBox Box1 = FirstItem.Box;
		const FBox Box2(Second.PlacementMin, Second.PlacementMin + Second.RotatedSize);

		// Check gap on each axis where they're adjacent
		bool bHasGap = false;
		for (int C = 0; C < 3; C++)
		{
			const double Gap1 = Box2.Min[C] - Box1.Max[C];
			const double Gap2 = Box1.Min[C] - Box2.Max[C];
			const double Gap = FMath::Max(Gap1, Gap2);
			if (Gap >= Padding[C] * 2.0 - KINDA_SMALL_NUMBER)
			{
				bHasGap = true;
			}
		}
		TestTrue(TEXT("Actual boxes have a gap of at least 2*padding on some axis"), bHasGap);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPaddingItemTooLargeWithPaddingTest,
	"PCGEx.Unit.Layout.Packing.Padding.ItemTooLargeWithPadding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExPaddingItemTooLargeWithPaddingTest::RunTest(const FString& Parameters)
{
	// 100x100x100 bin. Item 90x90x90 fits without padding but not with padding 10 (padded=110).
	TSharedPtr<FBP3DBin> Bin = CreateTestBin();

	FBP3DPlacementCandidate NoPadCandidate;
	const bool bFitsNoPad = Bin->EvaluatePlacement(FVector(90, 90, 90), FVector::ZeroVector, 0, FRotator::ZeroRotator, NoPadCandidate);
	TestTrue(TEXT("90^3 fits without padding"), bFitsNoPad);

	FBP3DPlacementCandidate PadCandidate;
	const bool bFitsPad = Bin->EvaluatePlacement(FVector(90, 90, 90), FVector(10, 10, 10), 0, FRotator::ZeroRotator, PadCandidate);
	TestFalse(TEXT("90^3 with padding 10 doesn't fit (padded 110 > 100)"), bFitsPad);

	return true;
}

#pragma endregion
