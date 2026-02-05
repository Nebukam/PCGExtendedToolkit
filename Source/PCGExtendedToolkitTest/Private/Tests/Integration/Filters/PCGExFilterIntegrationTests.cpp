// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Fixtures/PCGExTestContext.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"

/**
 * Integration tests for PCGEx point data infrastructure.
 *
 * NOTE: FPCGExContext cannot be safely instantiated outside the PCG execution system
 * (its destructor crashes). Therefore, tests that need FPointIO/FFacade must run
 * through actual PCG graph execution.
 *
 * For unit tests that need point data, use FSimplePointDataFactory which creates
 * UPCGBasePointData directly without context dependencies.
 */

#pragma region Simple Point Data Tests

/**
 * Test basic point data creation without full context
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimplePointDataTest,
	"PCGEx.Integration.SimplePointData.Creation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimplePointDataTest::RunTest(const FString& Parameters)
{
	// Create point data without any context infrastructure
	UPCGBasePointData* Data = PCGExTest::FSimplePointDataFactory::CreateSequential(10, 100.0);
	TestNotNull(TEXT("Point data should be created"), Data);

	if (!Data) { return false; }

	TestEqual(TEXT("Should have 10 points"), Data->GetNumPoints(), 10);

	// Verify positions
	TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector ExpectedPos(i * 100.0, 0.0, 0.0);
		const FVector ActualPos = Transforms[i].GetLocation();
		TestTrue(
			FString::Printf(TEXT("Point %d should be at X=%f"), i, i * 100.0),
			ActualPos.Equals(ExpectedPos, KINDA_SMALL_NUMBER)
		);
	}

	return true;
}

/**
 * Test grid point data creation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleGridDataTest,
	"PCGEx.Integration.SimplePointData.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleGridDataTest::RunTest(const FString& Parameters)
{
	UPCGBasePointData* Data = PCGExTest::FSimplePointDataFactory::CreateGrid(
		FVector::ZeroVector,
		FVector(100.0, 100.0, 100.0),
		3, 3, 1
	);
	TestNotNull(TEXT("Grid data should be created"), Data);

	if (!Data) { return false; }

	TestEqual(TEXT("Should have 9 points (3x3)"), Data->GetNumPoints(), 9);

	// Verify corner positions
	TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

	// First point at origin
	TestTrue(TEXT("First point should be at origin"),
		Transforms[0].GetLocation().Equals(FVector::ZeroVector, KINDA_SMALL_NUMBER));

	// Last point at (200, 200, 0)
	TestTrue(TEXT("Last point should be at (200, 200, 0)"),
		Transforms[8].GetLocation().Equals(FVector(200.0, 200.0, 0.0), KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test 3D grid point data creation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleGrid3DDataTest,
	"PCGEx.Integration.SimplePointData.Grid3D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleGrid3DDataTest::RunTest(const FString& Parameters)
{
	UPCGBasePointData* Data = PCGExTest::FSimplePointDataFactory::CreateGrid(
		FVector(100.0, 200.0, 300.0), // Origin offset
		FVector(50.0, 50.0, 50.0),    // Spacing
		2, 2, 2                        // 2x2x2 = 8 points
	);
	TestNotNull(TEXT("3D grid data should be created"), Data);

	if (!Data) { return false; }

	TestEqual(TEXT("Should have 8 points (2x2x2)"), Data->GetNumPoints(), 8);

	// Verify origin point
	TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();
	TestTrue(TEXT("First point should be at origin"),
		Transforms[0].GetLocation().Equals(FVector(100.0, 200.0, 300.0), KINDA_SMALL_NUMBER));

	return true;
}

/**
 * Test random point data with reproducible seed
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleRandomDataTest,
	"PCGEx.Integration.SimplePointData.Random",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleRandomDataTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(-500), FVector(500));

	UPCGBasePointData* Data1 = PCGExTest::FSimplePointDataFactory::CreateRandom(Bounds, 20, 12345u);
	UPCGBasePointData* Data2 = PCGExTest::FSimplePointDataFactory::CreateRandom(Bounds, 20, 12345u);

	TestNotNull(TEXT("Random data should be created"), Data1);
	TestNotNull(TEXT("Second random data should be created"), Data2);

	if (!Data1 || !Data2) { return false; }

	// Same seed should produce same positions
	TConstPCGValueRange<FTransform> Transforms1 = Data1->GetConstTransformValueRange();
	TConstPCGValueRange<FTransform> Transforms2 = Data2->GetConstTransformValueRange();

	for (int32 i = 0; i < 20; ++i)
	{
		TestTrue(
			FString::Printf(TEXT("Point %d should match with same seed"), i),
			Transforms1[i].GetLocation().Equals(Transforms2[i].GetLocation(), KINDA_SMALL_NUMBER)
		);
	}

	return true;
}

/**
 * Test different seeds produce different positions
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleRandomDifferentSeedsTest,
	"PCGEx.Integration.SimplePointData.RandomDifferentSeeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleRandomDifferentSeedsTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(-500), FVector(500));

	UPCGBasePointData* Data1 = PCGExTest::FSimplePointDataFactory::CreateRandom(Bounds, 10, 11111u);
	UPCGBasePointData* Data2 = PCGExTest::FSimplePointDataFactory::CreateRandom(Bounds, 10, 22222u);

	TestNotNull(TEXT("First random data should be created"), Data1);
	TestNotNull(TEXT("Second random data should be created"), Data2);

	if (!Data1 || !Data2) { return false; }

	// Different seeds should produce different positions
	TConstPCGValueRange<FTransform> Transforms1 = Data1->GetConstTransformValueRange();
	TConstPCGValueRange<FTransform> Transforms2 = Data2->GetConstTransformValueRange();

	// At least some points should be different
	int32 DifferentCount = 0;
	for (int32 i = 0; i < 10; ++i)
	{
		if (!Transforms1[i].GetLocation().Equals(Transforms2[i].GetLocation(), KINDA_SMALL_NUMBER))
		{
			DifferentCount++;
		}
	}

	TestTrue(TEXT("Different seeds should produce mostly different positions"), DifferentCount > 5);

	return true;
}

/**
 * Test point data bounds are respected
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimpleRandomBoundsTest,
	"PCGEx.Integration.SimplePointData.RandomBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimpleRandomBoundsTest::RunTest(const FString& Parameters)
{
	const FBox Bounds(FVector(-100, -200, -300), FVector(100, 200, 300));

	UPCGBasePointData* Data = PCGExTest::FSimplePointDataFactory::CreateRandom(Bounds, 100, 54321u);
	TestNotNull(TEXT("Random data should be created"), Data);

	if (!Data) { return false; }

	TConstPCGValueRange<FTransform> Transforms = Data->GetConstTransformValueRange();

	// All points should be within bounds
	for (int32 i = 0; i < 100; ++i)
	{
		const FVector Pos = Transforms[i].GetLocation();
		TestTrue(
			FString::Printf(TEXT("Point %d X should be within bounds"), i),
			Pos.X >= -100.0 && Pos.X <= 100.0
		);
		TestTrue(
			FString::Printf(TEXT("Point %d Y should be within bounds"), i),
			Pos.Y >= -200.0 && Pos.Y <= 200.0
		);
		TestTrue(
			FString::Printf(TEXT("Point %d Z should be within bounds"), i),
			Pos.Z >= -300.0 && Pos.Z <= 300.0
		);
	}

	return true;
}

/**
 * Test empty/zero point data requests
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimplePointDataEdgeCasesTest,
	"PCGEx.Integration.SimplePointData.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimplePointDataEdgeCasesTest::RunTest(const FString& Parameters)
{
	// Zero points should return nullptr
	UPCGBasePointData* ZeroData = PCGExTest::FSimplePointDataFactory::CreateSequential(0);
	TestNull(TEXT("Zero points should return nullptr"), ZeroData);

	// Negative points should return nullptr
	UPCGBasePointData* NegativeData = PCGExTest::FSimplePointDataFactory::CreateSequential(-5);
	TestNull(TEXT("Negative points should return nullptr"), NegativeData);

	// Single point should work
	UPCGBasePointData* SingleData = PCGExTest::FSimplePointDataFactory::CreateSequential(1);
	TestNotNull(TEXT("Single point should work"), SingleData);
	if (SingleData)
	{
		TestEqual(TEXT("Should have 1 point"), SingleData->GetNumPoints(), 1);
	}

	return true;
}

#pragma endregion

#pragma region Point Data Attribute Tests

/**
 * Test creating attributes on point data
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSimplePointDataAttributesTest,
	"PCGEx.Integration.SimplePointData.Attributes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSimplePointDataAttributesTest::RunTest(const FString& Parameters)
{
	UPCGBasePointData* Data = PCGExTest::FSimplePointDataFactory::CreateSequential(5);
	TestNotNull(TEXT("Point data should be created"), Data);

	if (!Data) { return false; }

	// Create a float attribute
	UPCGMetadata* Metadata = Data->MutableMetadata();
	TestNotNull(TEXT("Metadata should exist"), Metadata);

	if (!Metadata) { return false; }

	FPCGMetadataAttribute<float>* FloatAttr = Metadata->CreateAttribute<float>(
		FName(TEXT("TestFloat")), 0.0f, true, false
	);
	TestNotNull(TEXT("Float attribute should be created"), FloatAttr);

	if (!FloatAttr) { return false; }

	// Set values
	for (int32 i = 0; i < 5; ++i)
	{
		FloatAttr->SetValue(Data->GetMetadataEntry(i), static_cast<float>(i) * 10.0f);
	}

	// Read back and verify
	for (int32 i = 0; i < 5; ++i)
	{
		const float Expected = static_cast<float>(i) * 10.0f;
		const float Actual = FloatAttr->GetValueFromItemKey(Data->GetMetadataEntry(i));
		TestEqual(
			FString::Printf(TEXT("Float attribute at index %d"), i),
			Actual,
			Expected
		);
	}

	return true;
}

#pragma endregion

#pragma region Test Context Tests

/**
 * Test that FTestContext creates valid infrastructure
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTestContextWorldTest,
	"PCGEx.Integration.TestContext.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTestContextWorldTest::RunTest(const FString& Parameters)
{
	PCGExTest::FScopedTestContext TestCtx;
	TestTrue(TEXT("Test context should initialize"), TestCtx.IsValid());

	if (!TestCtx.IsValid()) { return false; }

	// World infrastructure should exist
	TestNotNull(TEXT("World should exist"), TestCtx->GetWorld());
	TestNotNull(TEXT("Actor should exist"), TestCtx->GetActor());
	TestNotNull(TEXT("PCGComponent should exist"), TestCtx->GetPCGComponent());
	TestNotNull(TEXT("Context should exist"), TestCtx->GetContext());

	return true;
}

/**
 * Test facade creation through test context
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExTestContextFacadeTest,
	"PCGEx.Integration.TestContext.FacadeCreation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExTestContextFacadeTest::RunTest(const FString& Parameters)
{
	PCGExTest::FScopedTestContext TestCtx;
	if (!TestCtx.IsValid()) { return false; }

	TSharedPtr<PCGExData::FFacade> Facade = TestCtx->CreateFacade(10, 100.0);
	TestNotNull(TEXT("Facade should be created"), Facade.Get());

	if (!Facade) { return false; }

	TestEqual(TEXT("Should have 10 points"), Facade->GetNum(PCGExData::EIOSide::In), 10);
	TestTrue(TEXT("Data should be valid"), Facade->IsDataValid(PCGExData::EIOSide::In));

	return true;
}

#pragma endregion
