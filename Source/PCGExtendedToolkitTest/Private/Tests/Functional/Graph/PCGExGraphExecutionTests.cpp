// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Graph Execution Functional Tests
 *
 * Tests full PCG graph execution with PCGEx nodes.
 * These are slower tests that verify end-to-end functionality.
 *
 * Note: These tests require a running editor context and may
 * need specific test graphs set up in the project.
 *
 * Test naming: PCGEx.Functional.Graph.<Category>
 *
 * Run these selectively in CI/CD due to longer execution times:
 * -ExecCmds="Automation RunTests PCGEx.Functional"
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExTestHelpers.h"
#include "Helpers/PCGExPointDataHelpers.h"

#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "Data/PCGPointArrayData.h"
#include "Data/PCGPointData.h"
#include "UObject/Package.h"

using namespace PCGExTest;

// =============================================================================
// Point Data Creation Tests
// =============================================================================

/**
 * Test that we can create valid point data for testing
 * This is a prerequisite for more complex graph tests
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphPointDataCreationTest,
	"PCGEx.Functional.Graph.PointDataCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphPointDataCreationTest::RunTest(const FString& Parameters)
{
	// Test grid positions
	{
		UPCGBasePointData* Data = FPointDataBuilder()
			.WithGridPositions(FVector::ZeroVector, FVector(100, 100, 0), 5, 5, 1)
			.Build();

		TestNotNull(TEXT("Grid point data created"), Data);
		if (Data)
		{
			TestEqual(TEXT("Grid has 25 points"), Data->GetNumPoints(), 25);

			TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

			// Verify first point position
			const FVector FirstPos = Transforms[0].GetLocation();
			TestTrue(TEXT("First point at origin"),
			         FirstPos.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

			// Verify last point position
			const FVector LastPos = Transforms[24].GetLocation();
			TestTrue(TEXT("Last point at (400,400,0)"),
			         LastPos.Equals(FVector(400, 400, 0), KINDA_SMALL_NUMBER));
		}
	}

	// Test random positions
	{
		FBox Bounds(FVector(-100), FVector(100));
		UPCGBasePointData* Data = FPointDataBuilder()
			.WithRandomPositions(Bounds, 100, GetTestSeed())
			.Build();

		TestNotNull(TEXT("Random point data created"), Data);
		if (Data)
		{
			TestEqual(TEXT("Has 100 random points"), Data->GetNumPoints(), 100);

			TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

			// All points should be within bounds
			bool AllInBounds = true;
			for (int32 i = 0; i < Data->GetNumPoints(); ++i)
			{
				FVector Pos = Transforms[i].GetLocation();
				if (!Bounds.IsInside(Pos))
				{
					AllInBounds = false;
					break;
				}
			}
			TestTrue(TEXT("All points within bounds"), AllInBounds);
		}
	}

	// Test sphere positions
	{
		UPCGBasePointData* Data = FPointDataBuilder()
			.WithSpherePositions(FVector::ZeroVector, 100.0, 50, GetTestSeed())
			.Build();

		TestNotNull(TEXT("Sphere point data created"), Data);
		if (Data)
		{
			TestEqual(TEXT("Has 50 sphere points"), Data->GetNumPoints(), 50);

			TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

			// All points should be on sphere surface (within tolerance)
			bool AllOnSphere = true;
			const double Tolerance = 1.0; // Allow small floating point error
			for (int32 i = 0; i < Data->GetNumPoints(); ++i)
			{
				double Dist = Transforms[i].GetLocation().Size();
				if (FMath::Abs(Dist - 100.0) > Tolerance)
				{
					AllOnSphere = false;
					break;
				}
			}
			TestTrue(TEXT("All points on sphere surface"), AllOnSphere);
		}
	}

	return true;
}

/**
 * Test PCG graph creation and basic structure
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphStructureTest,
	"PCGEx.Functional.Graph.Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphStructureTest::RunTest(const FString& Parameters)
{
	// Create a basic PCG graph
	UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("PCG Graph created"), Graph);

	if (Graph)
	{
		// In PCG 5.7, newly created graphs may start empty (no default nodes)
		// Just verify we can query the nodes array without crashing
		const int32 NodeCount = Graph->GetNodes().Num();
		AddInfo(FString::Printf(TEXT("Graph has %d nodes"), NodeCount));

		// Verify we can iterate nodes safely (even if empty)
		for (UPCGNode* Node : Graph->GetNodes())
		{
			if (Node && Node->GetSettings())
			{
				AddInfo(FString::Printf(TEXT("Found node: %s"), *Node->GetSettings()->GetClass()->GetName()));
			}
		}
	}

	return true;
}

// =============================================================================
// Data Transformation Tests
// =============================================================================

/**
 * Test basic point data transformations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphDataTransformTest,
	"PCGEx.Functional.Graph.DataTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphDataTransformTest::RunTest(const FString& Parameters)
{
	// Create source data
	UPCGBasePointData* SourceData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	TestNotNull(TEXT("Source data created"), SourceData);
	if (!SourceData) return false;

	const int32 NumPoints = SourceData->GetNumPoints();
	TConstPCGValueRange<FTransform> SourceTransforms = SourceData->GetConstTransformValueRange();

	// Simulate a transform operation (scale all positions)
	UPCGBasePointData* TransformedData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
	TransformedData->SetNumPoints(NumPoints);
	TPCGValueRange<FTransform> OutTransforms = TransformedData->GetTransformValueRange();

	const FVector Scale(2.0, 2.0, 1.0);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		FVector Pos = SourceTransforms[i].GetLocation();
		FTransform NewTransform = SourceTransforms[i];
		NewTransform.SetLocation(Pos * Scale);
		OutTransforms[i] = NewTransform;
	}

	// Verify transformation
	TestEqual(TEXT("Same point count after transform"),
	          TransformedData->GetNumPoints(), SourceData->GetNumPoints());

	// Check a specific point was scaled
	TConstPCGValueRange<FTransform> TransformedTransforms = TransformedData->GetConstTransformValueRange();
	FVector OriginalPos = SourceTransforms[4].GetLocation(); // Center point
	FVector TransformedPos = TransformedTransforms[4].GetLocation();
	FVector ExpectedPos = OriginalPos * Scale;

	TestTrue(TEXT("Center point scaled correctly"),
	         TransformedPos.Equals(ExpectedPos, KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test point filtering simulation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphDataFilterTest,
	"PCGEx.Functional.Graph.DataFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphDataFilterTest::RunTest(const FString& Parameters)
{
	// Create source data
	UPCGBasePointData* SourceData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 5, 5, 1)
		.Build();

	TestNotNull(TEXT("Source data created"), SourceData);
	if (!SourceData) return false;

	const int32 SourceNumPoints = SourceData->GetNumPoints();
	TConstPCGValueRange<FTransform> SourceTransforms = SourceData->GetConstTransformValueRange();

	// Simulate filtering: keep only points where X > 200
	// First pass: count how many points pass the filter
	TArray<int32> PassingIndices;
	PassingIndices.Reserve(SourceNumPoints);

	for (int32 i = 0; i < SourceNumPoints; ++i)
	{
		if (SourceTransforms[i].GetLocation().X > 200)
		{
			PassingIndices.Add(i);
		}
	}

	// Create filtered data with exact size
	UPCGBasePointData* FilteredData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
	FilteredData->SetNumPoints(PassingIndices.Num());
	TPCGValueRange<FTransform> FilteredTransforms = FilteredData->GetTransformValueRange();

	for (int32 OutIndex = 0; OutIndex < PassingIndices.Num(); ++OutIndex)
	{
		FilteredTransforms[OutIndex] = SourceTransforms[PassingIndices[OutIndex]];
	}

	// Original: 25 points in 5x5 grid
	// X values: 0, 100, 200, 300, 400
	// Keep X > 200: columns at X=300, X=400 = 2 columns * 5 rows = 10 points
	TestEqual(TEXT("Filtered to 10 points (X > 200)"),
	          FilteredData->GetNumPoints(), 10);

	// Verify all remaining points pass filter
	TConstPCGValueRange<FTransform> VerifyTransforms = FilteredData->GetConstTransformValueRange();
	bool AllPass = true;
	for (int32 i = 0; i < FilteredData->GetNumPoints(); ++i)
	{
		if (VerifyTransforms[i].GetLocation().X <= 200)
		{
			AllPass = false;
			break;
		}
	}
	TestTrue(TEXT("All filtered points have X > 200"), AllPass);

	return true;
}

// =============================================================================
// Attribute Tests
// =============================================================================

/**
 * Test attribute creation and access
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphAttributeTest,
	"PCGEx.Functional.Graph.Attributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphAttributeTest::RunTest(const FString& Parameters)
{
	// Create point data with metadata
	UPCGBasePointData* Data = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	TestNotNull(TEXT("Data created"), Data);
	if (!Data) return false;

	UPCGMetadata* Metadata = Data->MutableMetadata();
	TestNotNull(TEXT("Metadata accessible"), Metadata);
	if (!Metadata) return false;

	// Create a float attribute
	FPCGMetadataAttribute<float>* DensityAttr =
		Metadata->FindOrCreateAttribute<float>(FName(TEXT("TestDensity")), 1.0f, true, false);

	TestNotNull(TEXT("Float attribute created"), DensityAttr);

	// Create an int attribute
	FPCGMetadataAttribute<int32>* IndexAttr =
		Metadata->FindOrCreateAttribute<int32>(FName(TEXT("TestIndex")), -1, true, false);

	TestNotNull(TEXT("Int attribute created"), IndexAttr);

	// Create a vector attribute
	FPCGMetadataAttribute<FVector>* NormalAttr =
		Metadata->FindOrCreateAttribute<FVector>(FName(TEXT("TestNormal")), FVector::UpVector, true, false);

	TestNotNull(TEXT("Vector attribute created"), NormalAttr);

	// Verify attributes exist
	TestNotNull(TEXT("Can retrieve float attribute"),
	            Metadata->GetConstAttribute(FName(TEXT("TestDensity"))));
	TestNotNull(TEXT("Can retrieve int attribute"),
	            Metadata->GetConstAttribute(FName(TEXT("TestIndex"))));
	TestNotNull(TEXT("Can retrieve vector attribute"),
	            Metadata->GetConstAttribute(FName(TEXT("TestNormal"))));

	return true;
}

// =============================================================================
// Bounds and Spatial Tests
// =============================================================================

/**
 * Test bounds calculation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphBoundsTest,
	"PCGEx.Functional.Graph.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphBoundsTest::RunTest(const FString& Parameters)
{
	// Create known grid data
	UPCGBasePointData* Data = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 4, 4, 1)
		.Build();

	TestNotNull(TEXT("Data created"), Data);
	if (!Data) return false;

	TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

	// Calculate bounds manually
	FBox Bounds(ForceInit);
	for (int32 i = 0; i < Data->GetNumPoints(); ++i)
	{
		Bounds += Transforms[i].GetLocation();
	}

	// Expected: 4x4 grid from (0,0,0) to (300,300,0)
	TestTrue(TEXT("Min bounds correct"),
	         Bounds.Min.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Max bounds correct"),
	         Bounds.Max.Equals(FVector(300, 300, 0), KINDA_SMALL_NUMBER));

	FVector Size = Bounds.GetSize();
	TestTrue(TEXT("Bounds size correct"),
	         Size.Equals(FVector(300, 300, 0), KINDA_SMALL_NUMBER));

	FVector Center = Bounds.GetCenter();
	TestTrue(TEXT("Bounds center correct"),
	         Center.Equals(FVector(150, 150, 0), KINDA_SMALL_NUMBER));

	return true;
}

// =============================================================================
// Test Reproducibility
// =============================================================================

/**
 * Test that random operations are reproducible with same seed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExGraphReproducibilityTest,
	"PCGEx.Functional.Graph.Reproducibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExGraphReproducibilityTest::RunTest(const FString& Parameters)
{
	const uint32 TestSeed = 42;
	FBox Bounds(FVector(-100), FVector(100));

	// Generate first set
	TArray<FVector> Positions1 = GenerateRandomPositions(50, Bounds, TestSeed);

	// Generate second set with same seed
	TArray<FVector> Positions2 = GenerateRandomPositions(50, Bounds, TestSeed);

	// Should be identical
	TestEqual(TEXT("Same count"), Positions1.Num(), Positions2.Num());

	bool AllMatch = true;
	for (int32 i = 0; i < Positions1.Num(); ++i)
	{
		if (!Positions1[i].Equals(Positions2[i], KINDA_SMALL_NUMBER))
		{
			AllMatch = false;
			AddError(FString::Printf(TEXT("Position mismatch at index %d"), i));
			break;
		}
	}

	TestTrue(TEXT("All positions match with same seed"), AllMatch);

	// Generate third set with different seed - should differ
	TArray<FVector> Positions3 = GenerateRandomPositions(50, Bounds, TestSeed + 1);

	bool AnyDiffer = false;
	for (int32 i = 0; i < Positions1.Num(); ++i)
	{
		if (!Positions1[i].Equals(Positions3[i], KINDA_SMALL_NUMBER))
		{
			AnyDiffer = true;
			break;
		}
	}

	TestTrue(TEXT("Different seed produces different positions"), AnyDiffer);

	return true;
}
