// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Data Flow Functional Tests
 *
 * Tests the actual data flow between PCGEx components to reproduce
 * and diagnose the "random missing data" bug.
 *
 * Test naming: PCGEx.Functional.DataFlow.<Scenario>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExTestHelpers.h"
#include "Helpers/PCGExPointDataHelpers.h"
#include "Fixtures/PCGExTestFixtures.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Data/PCGPointArrayData.h"
#include "UObject/Package.h"

// PCGEx includes
#include "Core/PCGExContext.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExData.h"

using namespace PCGExTest;

// =============================================================================
// FPointIO Initialization Tests
// =============================================================================

/**
 * Test FPointIO::InitializeOutput with EIOInit::New (Collapse mode pattern)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPointIONewInitTest,
	"PCGEx.Functional.DataFlow.PointIONewInit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPointIONewInitTest::RunTest(const FString& Parameters)
{
	// Setup test fixture
	FTestFixture Fixture;
	Fixture.Setup();

	if (!Fixture.GetWorld() || !Fixture.GetPCGComponent())
	{
		AddError(TEXT("Failed to setup test fixture"));
		return false;
	}

	// Create input point data (simulating what comes into PointsToBounds)
	UPCGBasePointData* InputData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	TestNotNull(TEXT("Input data created"), InputData);
	if (!InputData) return false;

	TestEqual(TEXT("Input has 9 points"), InputData->GetNumPoints(), 9);

	AddInfo(FString::Printf(TEXT("Created input with %d points"), InputData->GetNumPoints()));

	// Note: Full FPointIO testing requires a valid context which needs more setup
	// This test verifies the input data creation works

	Fixture.Teardown();
	return true;
}

/**
 * Test that point data can be properly duplicated and modified
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPointDataDuplicateTest,
	"PCGEx.Functional.DataFlow.PointDataDuplicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPointDataDuplicateTest::RunTest(const FString& Parameters)
{
	// Create source data
	UPCGBasePointData* SourceData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	TestNotNull(TEXT("Source data created"), SourceData);
	if (!SourceData) return false;

	// Create new output data (simulating Collapse mode)
	UPCGPointArrayData* OutputData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Output data created"), OutputData);
	if (!OutputData) return false;

	// Set 1 point (like Collapse mode)
	OutputData->SetNumPoints(1);
	TestEqual(TEXT("Output has 1 point"), OutputData->GetNumPoints(), 1);

	// Set the transform
	TPCGValueRange<FTransform> OutTransforms = OutputData->GetTransformValueRange(false);
	OutTransforms[0] = FTransform(FVector(150, 150, 0)); // Center of the 3x3 grid

	// Verify transform was set
	TConstPCGValueRange<FTransform> VerifyTransforms = OutputData->GetConstTransformValueRange();
	FVector OutLocation = VerifyTransforms[0].GetLocation();

	TestTrue(TEXT("Transform location set correctly"),
		OutLocation.Equals(FVector(150, 150, 0), KINDA_SMALL_NUMBER));

	// Check if data is considered "empty"
	bool IsEmpty = OutputData->IsEmpty();
	AddInfo(FString::Printf(TEXT("OutputData->IsEmpty() = %s"), IsEmpty ? TEXT("true") : TEXT("false")));
	TestFalse(TEXT("Output data should not be empty"), IsEmpty);

	return true;
}

/**
 * Test the staging condition that determines if data gets staged
 *
 * This tests the check in FPointIO::StageOutput:
 * if (!IsEnabled() || !Out || (!bAllowEmptyOutput && Out->IsEmpty())) { return false; }
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExStagingConditionTest,
	"PCGEx.Functional.DataFlow.StagingCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExStagingConditionTest::RunTest(const FString& Parameters)
{
	// Test 1: Data with points should pass
	{
		UPCGPointArrayData* DataWithPoints = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		DataWithPoints->SetNumPoints(1);

		bool IsEmpty = DataWithPoints->IsEmpty();
		AddInfo(FString::Printf(TEXT("1 point data IsEmpty: %s"), IsEmpty ? TEXT("true") : TEXT("false")));
		TestFalse(TEXT("Data with 1 point should not be empty"), IsEmpty);
	}

	// Test 2: Empty data should fail staging (unless bAllowEmptyOutput)
	{
		UPCGPointArrayData* EmptyData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		// Don't set any points

		bool IsEmpty = EmptyData->IsEmpty();
		AddInfo(FString::Printf(TEXT("Empty data IsEmpty: %s"), IsEmpty ? TEXT("true") : TEXT("false")));
		TestTrue(TEXT("Data with 0 points should be empty"), IsEmpty);
	}

	// Test 3: Data with points allocated but at 0
	{
		UPCGPointArrayData* ZeroPointData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
		ZeroPointData->SetNumPoints(0);

		bool IsEmpty = ZeroPointData->IsEmpty();
		int32 NumPoints = ZeroPointData->GetNumPoints();
		AddInfo(FString::Printf(TEXT("SetNumPoints(0): IsEmpty=%s, NumPoints=%d"),
			IsEmpty ? TEXT("true") : TEXT("false"), NumPoints));
	}

	return true;
}

/**
 * Test the exact sequence of operations in PointsToBounds Collapse mode
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCollapseModeSequenceTest,
	"PCGEx.Functional.DataFlow.CollapseModeSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCollapseModeSequenceTest::RunTest(const FString& Parameters)
{
	// Simulate the sequence in PointsToBounds::FProcessor

	// Step 1: Create input (9 points in a 3x3 grid)
	UPCGBasePointData* InData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	TestNotNull(TEXT("Step 1: Input created"), InData);
	if (!InData) return false;
	AddInfo(FString::Printf(TEXT("Input: %d points"), InData->GetNumPoints()));

	// Step 2: Create output (like InitializeOutput(New))
	UPCGPointArrayData* OutData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Step 2: Output created"), OutData);

	// Step 3: Allocate 1 point (like SetNumPointsAllocated(OutData, 1))
	OutData->SetNumPoints(1);
	AddInfo(FString::Printf(TEXT("After SetNumPoints(1): %d points, IsEmpty=%s"),
		OutData->GetNumPoints(), OutData->IsEmpty() ? TEXT("true") : TEXT("false")));

	// Step 4: InheritPoints would copy properties from input point 0 to output point 0
	// For this test, we'll manually set the transform

	// Step 5: Check if MetadataBlender would fail here (the potential issue path)
	// In the real code, if bBlendProperties=true and Init fails, CompleteWork returns early
	// BEFORE setting the transform/bounds

	// Step 6: Set transform and bounds (what happens if we DON'T return early)
	{
		TPCGValueRange<FTransform> OutTransforms = OutData->GetTransformValueRange(false);
		TPCGValueRange<FVector> OutBoundsMin = OutData->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = OutData->GetBoundsMaxValueRange(false);

		FVector Center(100, 100, 0);
		OutTransforms[0] = FTransform(FQuat::Identity, Center);
		OutBoundsMin[0] = FVector(0, 0, 0) - Center;
		OutBoundsMax[0] = FVector(200, 200, 0) - Center;

		AddInfo(TEXT("Transform and bounds set successfully"));
	}

	// Step 7: Verify output is valid for staging
	TestFalse(TEXT("Output should not be empty after setup"), OutData->IsEmpty());
	TestEqual(TEXT("Output should have 1 point"), OutData->GetNumPoints(), 1);

	// Verify the transform was actually set
	TConstPCGValueRange<FTransform> VerifyTransforms = OutData->GetConstTransformValueRange();
	FVector VerifyLocation = VerifyTransforms[0].GetLocation();
	TestTrue(TEXT("Transform location correct"),
		VerifyLocation.Equals(FVector(100, 100, 0), KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test what happens when CompleteWork returns early (the MetadataBlender failure path)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExCompleteWorkEarlyReturnTest,
	"PCGEx.Functional.DataFlow.CompleteWorkEarlyReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExCompleteWorkEarlyReturnTest::RunTest(const FString& Parameters)
{
	// Simulate the sequence when CompleteWork returns early due to MetadataBlender failure

	// Step 1: Create input
	UPCGBasePointData* InData = FPointDataBuilder()
		.WithGridPositions(FVector::ZeroVector, FVector(100), 3, 3, 1)
		.Build();

	if (!InData) return false;

	// Step 2: Create output
	UPCGPointArrayData* OutData = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);

	// Step 3: Allocate 1 point
	OutData->SetNumPoints(1);

	// Step 4: InheritPoints - copy input point 0 to output point 0
	// This copies the transform from input
	{
		TConstPCGValueRange<FTransform> InTransforms = InData->GetConstTransformValueRange();
		TPCGValueRange<FTransform> OutTransforms = OutData->GetTransformValueRange(false);
		OutTransforms[0] = InTransforms[0]; // Copy first input point's transform
	}

	// SIMULATE EARLY RETURN: Skip setting the computed bounds
	// bIsProcessorValid = false;
	// return;

	// Check state after early return
	AddInfo(TEXT("Simulating CompleteWork early return (MetadataBlender failure)"));
	AddInfo(FString::Printf(TEXT("Output: %d points, IsEmpty=%s"),
		OutData->GetNumPoints(), OutData->IsEmpty() ? TEXT("true") : TEXT("false")));

	// The output has 1 point with the COPIED transform from input (first point at origin)
	// NOT the computed bounds center
	TConstPCGValueRange<FTransform> VerifyTransforms = OutData->GetConstTransformValueRange();
	FVector OutLocation = VerifyTransforms[0].GetLocation();
	AddInfo(FString::Printf(TEXT("Output point location: (%.1f, %.1f, %.1f)"),
		OutLocation.X, OutLocation.Y, OutLocation.Z));

	// The output IS valid (has 1 point) but has WRONG data (first input point, not bounds)
	TestFalse(TEXT("Output not empty"), OutData->IsEmpty());
	TestTrue(TEXT("Output location is at origin (copied from input, not computed)"),
		OutLocation.Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

	// This output WOULD be staged and passed to downstream node
	// The downstream node would see 1 point at origin, not the computed bounds!
	AddInfo(TEXT("Result: Output would be staged with INCORRECT data (copied input, not bounds)"));

	return true;
}

/**
 * Test timing/ordering of output operations
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExOutputTimingTest,
	"PCGEx.Functional.DataFlow.OutputTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExOutputTimingTest::RunTest(const FString& Parameters)
{
	// This test investigates whether there's a timing issue where
	// IsEmpty() returns true at the wrong time

	UPCGPointArrayData* Data = NewObject<UPCGPointArrayData>(GetTransientPackage(), NAME_None, RF_Transient);

	// Check initial state
	AddInfo(FString::Printf(TEXT("Initial: NumPoints=%d, IsEmpty=%s"),
		Data->GetNumPoints(), Data->IsEmpty() ? TEXT("true") : TEXT("false")));

	// After SetNumPoints
	Data->SetNumPoints(1);
	AddInfo(FString::Printf(TEXT("After SetNumPoints(1): NumPoints=%d, IsEmpty=%s"),
		Data->GetNumPoints(), Data->IsEmpty() ? TEXT("true") : TEXT("false")));

	// Get transform range (this might allocate properties)
	TPCGValueRange<FTransform> Transforms = Data->GetTransformValueRange(false);
	AddInfo(FString::Printf(TEXT("After GetTransformValueRange: NumPoints=%d, IsEmpty=%s"),
		Data->GetNumPoints(), Data->IsEmpty() ? TEXT("true") : TEXT("false")));

	// Set the transform
	Transforms[0] = FTransform(FVector(100, 100, 0));
	AddInfo(FString::Printf(TEXT("After setting transform: NumPoints=%d, IsEmpty=%s"),
		Data->GetNumPoints(), Data->IsEmpty() ? TEXT("true") : TEXT("false")));

	// Final state
	TestFalse(TEXT("Data should not be empty at end"), Data->IsEmpty());
	TestEqual(TEXT("Data should have 1 point"), Data->GetNumPoints(), 1);

	return true;
}
