// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Constant Filter Unit Tests
 *
 * Tests the FPCGExConstantFilterConfig and filter logic.
 * These tests verify the configuration behavior without requiring
 * a full PCG context setup.
 *
 * Test naming convention: PCGEx.Unit.Filters.Constant.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Filters/Points/PCGExConstantFilter.h"

// =============================================================================
// Config Tests
// =============================================================================

/**
 * Test FPCGExConstantFilterConfig default values
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterConfigDefaultTest,
	"PCGEx.Unit.Filters.Constant.Config.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterConfigDefaultTest::RunTest(const FString& Parameters)
{
	FPCGExConstantFilterConfig Config;

	// Default value should be true (pass filter)
	TestTrue(TEXT("Default Value is true"), Config.Value);

	// Default invert should be false
	TestFalse(TEXT("Default bInvert is false"), Config.bInvert);

	return true;
}

/**
 * Test the constant filter logic simulation
 *
 * The actual filter computes: ConstantValue = bInvert ? !Value : Value
 * Then Test() always returns ConstantValue
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterLogicTest,
	"PCGEx.Unit.Filters.Constant.Logic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterLogicTest::RunTest(const FString& Parameters)
{
	// Simulate the filter logic: ConstantValue = bInvert ? !Value : Value
	auto ComputeConstantValue = [](const FPCGExConstantFilterConfig& Config) -> bool
	{
		return Config.bInvert ? !Config.Value : Config.Value;
	};

	// Case 1: Value=true, Invert=false -> true
	{
		FPCGExConstantFilterConfig Config;
		Config.Value = true;
		Config.bInvert = false;
		TestTrue(TEXT("true + no invert = true"), ComputeConstantValue(Config));
	}

	// Case 2: Value=false, Invert=false -> false
	{
		FPCGExConstantFilterConfig Config;
		Config.Value = false;
		Config.bInvert = false;
		TestFalse(TEXT("false + no invert = false"), ComputeConstantValue(Config));
	}

	// Case 3: Value=true, Invert=true -> false
	{
		FPCGExConstantFilterConfig Config;
		Config.Value = true;
		Config.bInvert = true;
		TestFalse(TEXT("true + invert = false"), ComputeConstantValue(Config));
	}

	// Case 4: Value=false, Invert=true -> true
	{
		FPCGExConstantFilterConfig Config;
		Config.Value = false;
		Config.bInvert = true;
		TestTrue(TEXT("false + invert = true"), ComputeConstantValue(Config));
	}

	return true;
}

/**
 * Test the filter with multiple point indices
 *
 * The constant filter should return the same value for all points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterConsistencyTest,
	"PCGEx.Unit.Filters.Constant.Consistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterConsistencyTest::RunTest(const FString& Parameters)
{
	// Simulate testing multiple point indices with the constant filter
	auto SimulateTest = [](const FPCGExConstantFilterConfig& Config, int32 PointIndex) -> bool
	{
		// The filter logic: Test() always returns ConstantValue
		const bool ConstantValue = Config.bInvert ? !Config.Value : Config.Value;
		return ConstantValue;
	};

	// Config that passes
	FPCGExConstantFilterConfig PassConfig;
	PassConfig.Value = true;
	PassConfig.bInvert = false;

	// All points should pass regardless of index
	for (int32 i = 0; i < 100; ++i)
	{
		TestTrue(FString::Printf(TEXT("Pass config: point %d passes"), i),
		         SimulateTest(PassConfig, i));
	}

	// Config that fails
	FPCGExConstantFilterConfig FailConfig;
	FailConfig.Value = false;
	FailConfig.bInvert = false;

	// All points should fail regardless of index
	for (int32 i = 0; i < 100; ++i)
	{
		TestFalse(FString::Printf(TEXT("Fail config: point %d fails"), i),
		          SimulateTest(FailConfig, i));
	}

	return true;
}

// =============================================================================
// Factory Tests
// =============================================================================

/**
 * Test UPCGExConstantFilterFactory creation
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterFactoryTest,
	"PCGEx.Unit.Filters.Constant.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterFactoryTest::RunTest(const FString& Parameters)
{
	// Create a filter factory
	UPCGExConstantFilterFactory* Factory = NewObject<UPCGExConstantFilterFactory>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Factory created"), Factory);

	if (Factory)
	{
		// Test config is accessible and modifiable
		Factory->Config.Value = true;
		TestTrue(TEXT("Config.Value can be set to true"), Factory->Config.Value);

		Factory->Config.Value = false;
		TestFalse(TEXT("Config.Value can be set to false"), Factory->Config.Value);

		Factory->Config.bInvert = true;
		TestTrue(TEXT("Config.bInvert can be set"), Factory->Config.bInvert);

		// Test factory can create filter
		TSharedPtr<PCGExPointFilter::IFilter> Filter = Factory->CreateFilter();
		TestTrue(TEXT("Filter created from factory"), Filter.IsValid());

		// Test factory reports collection evaluation support
		TestTrue(TEXT("Supports collection evaluation"),
		         Factory->SupportsCollectionEvaluation());

		// Test factory reports proxy evaluation support
		TestTrue(TEXT("Supports proxy evaluation"),
		         Factory->SupportsProxyEvaluation());
	}

	return true;
}

// =============================================================================
// Provider Settings Tests
// =============================================================================

/**
 * Test UPCGExConstantFilterProviderSettings
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterProviderSettingsTest,
	"PCGEx.Unit.Filters.Constant.ProviderSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterProviderSettingsTest::RunTest(const FString& Parameters)
{
	// Create provider settings
	UPCGExConstantFilterProviderSettings* Settings = NewObject<UPCGExConstantFilterProviderSettings>(GetTransientPackage(), NAME_None, RF_Transient);
	TestNotNull(TEXT("Provider settings created"), Settings);

	if (Settings)
	{
		// Test config is accessible
		Settings->Config.Value = true;
		TestTrue(TEXT("Config.Value accessible"), Settings->Config.Value);

		Settings->Config.bInvert = true;
		TestTrue(TEXT("Config.bInvert accessible"), Settings->Config.bInvert);

#if WITH_EDITOR
		// Test display name reflects config
		Settings->Config.Value = true;
		Settings->Config.bInvert = false;
		FString DisplayName = Settings->GetDisplayName();
		TestEqual(TEXT("Display name is 'Pass' when Value=true"), DisplayName, TEXT("Pass"));

		Settings->Config.Value = false;
		DisplayName = Settings->GetDisplayName();
		TestEqual(TEXT("Display name is 'Fail' when Value=false"), DisplayName, TEXT("Fail"));
#endif
	}

	return true;
}

// =============================================================================
// Use Case Tests
// =============================================================================

/**
 * Test use case: using constant filter as pass-through
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterUseCasePassThroughTest,
	"PCGEx.Unit.Filters.Constant.UseCase.PassThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterUseCasePassThroughTest::RunTest(const FString& Parameters)
{
	// Use case: Pass-through filter that lets all points through
	FPCGExConstantFilterConfig PassThroughConfig;
	PassThroughConfig.Value = true;
	PassThroughConfig.bInvert = false;

	// Simulate filtering 1000 points
	TArray<int32> PassingIndices;
	for (int32 i = 0; i < 1000; ++i)
	{
		const bool ConstantValue = PassThroughConfig.bInvert
			                           ? !PassThroughConfig.Value
			                           : PassThroughConfig.Value;
		if (ConstantValue)
		{
			PassingIndices.Add(i);
		}
	}

	// All 1000 points should pass
	TestEqual(TEXT("All 1000 points pass through"), PassingIndices.Num(), 1000);

	return true;
}

/**
 * Test use case: using constant filter to reject all points
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterUseCaseRejectAllTest,
	"PCGEx.Unit.Filters.Constant.UseCase.RejectAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterUseCaseRejectAllTest::RunTest(const FString& Parameters)
{
	// Use case: Reject all filter for debugging/testing
	FPCGExConstantFilterConfig RejectAllConfig;
	RejectAllConfig.Value = false;
	RejectAllConfig.bInvert = false;

	// Simulate filtering 1000 points
	TArray<int32> PassingIndices;
	for (int32 i = 0; i < 1000; ++i)
	{
		const bool ConstantValue = RejectAllConfig.bInvert
			                           ? !RejectAllConfig.Value
			                           : RejectAllConfig.Value;
		if (ConstantValue)
		{
			PassingIndices.Add(i);
		}
	}

	// No points should pass
	TestEqual(TEXT("No points pass reject-all filter"), PassingIndices.Num(), 0);

	return true;
}

/**
 * Test use case: using constant filter in AND group
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterUseCaseAndGroupTest,
	"PCGEx.Unit.Filters.Constant.UseCase.AndGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterUseCaseAndGroupTest::RunTest(const FString& Parameters)
{
	// Simulate: AND group with [ConstantPass, OtherFilter]
	// If constant is false, entire group fails regardless of other filters
	auto SimulateAndGroup = [](bool ConstantResult, bool OtherResult) -> bool
	{
		return ConstantResult && OtherResult;
	};

	// Constant pass (true) + other passes (true) = pass
	TestTrue(TEXT("AND: pass + pass = pass"),
	         SimulateAndGroup(true, true));

	// Constant pass (true) + other fails (false) = fail
	TestFalse(TEXT("AND: pass + fail = fail"),
	          SimulateAndGroup(true, false));

	// Constant fail (false) + other passes (true) = fail
	// This demonstrates using constant filter to disable an entire filter group
	TestFalse(TEXT("AND: fail + pass = fail (disables group)"),
	          SimulateAndGroup(false, true));

	return true;
}

/**
 * Test use case: using constant filter in OR group
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExConstantFilterUseCaseOrGroupTest,
	"PCGEx.Unit.Filters.Constant.UseCase.OrGroup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExConstantFilterUseCaseOrGroupTest::RunTest(const FString& Parameters)
{
	// Simulate: OR group with [ConstantPass, OtherFilter]
	// If constant is true, entire group passes regardless of other filters
	auto SimulateOrGroup = [](bool ConstantResult, bool OtherResult) -> bool
	{
		return ConstantResult || OtherResult;
	};

	// Constant pass (true) + other fails (false) = pass
	// This demonstrates using constant filter to force-pass an entire filter group
	TestTrue(TEXT("OR: pass + fail = pass (force-pass group)"),
	         SimulateOrGroup(true, false));

	// Constant fail (false) + other passes (true) = pass
	TestTrue(TEXT("OR: fail + pass = pass"),
	         SimulateOrGroup(false, true));

	// Constant fail (false) + other fails (false) = fail
	TestFalse(TEXT("OR: fail + fail = fail"),
	          SimulateOrGroup(false, false));

	return true;
}
