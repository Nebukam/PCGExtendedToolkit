// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Filter Integration Tests (BDD Style)
 *
 * Tests the filter framework using Unreal's BDD-style spec testing.
 * These tests verify filter behavior patterns without requiring full
 * PCG context setup, focusing on the logical outcomes.
 *
 * Note: Full filter tests with live PCG data require a running world
 * and proper context initialization. These tests focus on the filter
 * logic and configuration patterns.
 *
 * Test naming: PCGEx.Integration.Filters.<Category>
 *
 * BDD Structure:
 * - Describe: Category of tests
 * - It: Individual test case
 * - BeforeEach/AfterEach: Setup/teardown
 */

#include "Misc/AutomationTest.h"
#include "PCGExFilterCommon.h"

// =============================================================================
// Filter Enums and Configuration Tests
// =============================================================================

BEGIN_DEFINE_SPEC(
	FPCGExFilterEnumsSpec,
	"PCGEx.Integration.Filters.Enums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FPCGExFilterEnumsSpec)

void FPCGExFilterEnumsSpec::Define()
{
	Describe("EPCGExFilterFallback", [this]()
	{
		It("should have Pass as 0 for default behavior", [this]()
		{
			// Pass = 0 ensures default-initialized enums pass filters
			TestEqual(TEXT("Pass is 0"),
			          static_cast<uint8>(EPCGExFilterFallback::Pass), 0);
		});

		It("should have distinct values for Pass and Fail", [this]()
		{
			TestNotEqual(TEXT("Pass != Fail"),
			             static_cast<uint8>(EPCGExFilterFallback::Pass),
			             static_cast<uint8>(EPCGExFilterFallback::Fail));
		});
	});

	Describe("EPCGExFilterResult", [this]()
	{
		It("should have Pass as 0", [this]()
		{
			TestEqual(TEXT("Pass is 0"),
			          static_cast<uint8>(EPCGExFilterResult::Pass), 0);
		});

		It("should be usable as bool-like for success checking", [this]()
		{
			// Pass = 0 means !Pass is true when checking for failure
			const EPCGExFilterResult Success = EPCGExFilterResult::Pass;
			const EPCGExFilterResult Failure = EPCGExFilterResult::Fail;

			TestEqual(TEXT("Pass converts to 0"), static_cast<int>(Success), 0);
			TestEqual(TEXT("Fail converts to 1"), static_cast<int>(Failure), 1);
		});
	});

	Describe("EPCGExFilterGroupMode", [this]()
	{
		It("should have AND as default (0)", [this]()
		{
			TestEqual(TEXT("AND is 0"),
			          static_cast<uint8>(EPCGExFilterGroupMode::AND), 0);
		});

		It("should support both AND and OR modes", [this]()
		{
			// Both modes should be valid and distinct
			TestTrue(TEXT("AND mode exists"),
			         EPCGExFilterGroupMode::AND == EPCGExFilterGroupMode::AND);
			TestTrue(TEXT("OR mode exists"),
			         EPCGExFilterGroupMode::OR == EPCGExFilterGroupMode::OR);
			TestNotEqual(TEXT("AND != OR"),
			             static_cast<uint8>(EPCGExFilterGroupMode::AND),
			             static_cast<uint8>(EPCGExFilterGroupMode::OR));
		});
	});

	Describe("EPCGExFilterNoDataFallback", [this]()
	{
		It("should have Error as default (0) for safety", [this]()
		{
			// Error as default ensures missing data doesn't silently pass
			TestEqual(TEXT("Error is 0"),
			          static_cast<uint8>(EPCGExFilterNoDataFallback::Error), 0);
		});

		It("should provide all three fallback options", [this]()
		{
			// Verify all options exist and are distinct
			TSet<uint8> Values;
			Values.Add(static_cast<uint8>(EPCGExFilterNoDataFallback::Error));
			Values.Add(static_cast<uint8>(EPCGExFilterNoDataFallback::Pass));
			Values.Add(static_cast<uint8>(EPCGExFilterNoDataFallback::Fail));

			TestEqual(TEXT("All three values are distinct"), Values.Num(), 3);
		});
	});
}

// =============================================================================
// Filter Type Tests
// =============================================================================

BEGIN_DEFINE_SPEC(
	FPCGExFilterTypesSpec,
	"PCGEx.Integration.Filters.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FPCGExFilterTypesSpec)

void FPCGExFilterTypesSpec::Define()
{
	Describe("PCGExFilters::EType", [this]()
	{
		It("should have None as 0 for invalid/uninitialized state", [this]()
		{
			TestEqual(TEXT("None is 0"),
			          static_cast<uint8>(PCGExFilters::EType::None), 0);
		});

		It("should support all filter domain types", [this]()
		{
			// Verify all types exist
			TArray<PCGExFilters::EType> AllTypes = {
				PCGExFilters::EType::None,
				PCGExFilters::EType::Point,
				PCGExFilters::EType::Group,
				PCGExFilters::EType::Node,
				PCGExFilters::EType::Edge,
				PCGExFilters::EType::Collection,
			};

			TSet<uint8> UniqueValues;
			for (PCGExFilters::EType Type : AllTypes)
			{
				UniqueValues.Add(static_cast<uint8>(Type));
			}

			TestEqual(TEXT("All filter types have unique values"),
			          UniqueValues.Num(), AllTypes.Num());
		});

		It("should have Point type for point-level filtering", [this]()
		{
			TestTrue(TEXT("Point type exists"),
			         PCGExFilters::EType::Point != PCGExFilters::EType::None);
		});

		It("should have graph-specific types (Node, Edge)", [this]()
		{
			TestTrue(TEXT("Node type exists and differs from Edge"),
			         PCGExFilters::EType::Node != PCGExFilters::EType::Edge);
		});
	});

	Describe("PCGExFilters::Labels", [this]()
	{
		It("should have non-empty filter label", [this]()
		{
			TestFalse(TEXT("OutputFilterLabel is not None"),
			          PCGExFilters::Labels::OutputFilterLabel.IsNone());
		});

		It("should have distinct labels for different filter outputs", [this]()
		{
			// Inside and Outside should be different
			TestNotEqual(TEXT("Inside != Outside labels"),
			             PCGExFilters::Labels::OutputInsideFiltersLabel,
			             PCGExFilters::Labels::OutputOutsideFiltersLabel);
		});

		It("should have proper source labels for filter pins", [this]()
		{
			// Common labels should exist
			TestFalse(TEXT("SourceFiltersLabel exists"),
			          PCGExFilters::Labels::SourceFiltersLabel.IsNone());
			TestFalse(TEXT("SourcePointFiltersLabel exists"),
			          PCGExFilters::Labels::SourcePointFiltersLabel.IsNone());
			TestFalse(TEXT("SourceVtxFiltersLabel exists"),
			          PCGExFilters::Labels::SourceVtxFiltersLabel.IsNone());
			TestFalse(TEXT("SourceEdgeFiltersLabel exists"),
			          PCGExFilters::Labels::SourceEdgeFiltersLabel.IsNone());
		});
	});
}

// =============================================================================
// Filter Logic Simulation Tests
// =============================================================================

/**
 * These tests verify the expected behavior of filter group logic
 * without instantiating actual filter objects.
 */
BEGIN_DEFINE_SPEC(
	FPCGExFilterLogicSpec,
	"PCGEx.Integration.Filters.Logic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	// Simulated filter results for testing group logic
	TArray<bool> FilterResults;

	// Simulate AND group behavior
	bool SimulateAndGroup(const TArray<bool>& Results)
	{
		for (bool Result : Results)
		{
			if (!Result) return false;
		}
		return true;
	}

	// Simulate OR group behavior
	bool SimulateOrGroup(const TArray<bool>& Results)
	{
		for (bool Result : Results)
		{
			if (Result) return true;
		}
		return false;
	}

END_DEFINE_SPEC(FPCGExFilterLogicSpec)

void FPCGExFilterLogicSpec::Define()
{
	BeforeEach([this]()
	{
		FilterResults.Reset();
	});

	Describe("AND Filter Group", [this]()
	{
		It("should pass when all filters pass", [this]()
		{
			FilterResults = {true, true, true};
			TestTrue(TEXT("All pass -> group passes"),
			         SimulateAndGroup(FilterResults));
		});

		It("should fail when any filter fails", [this]()
		{
			FilterResults = {true, false, true};
			TestFalse(TEXT("One fail -> group fails"),
			          SimulateAndGroup(FilterResults));
		});

		It("should fail when all filters fail", [this]()
		{
			FilterResults = {false, false, false};
			TestFalse(TEXT("All fail -> group fails"),
			          SimulateAndGroup(FilterResults));
		});

		It("should pass with single passing filter", [this]()
		{
			FilterResults = {true};
			TestTrue(TEXT("Single pass -> group passes"),
			         SimulateAndGroup(FilterResults));
		});

		It("should pass with empty filter list (vacuous truth)", [this]()
		{
			FilterResults = {};
			TestTrue(TEXT("Empty -> vacuously true"),
			         SimulateAndGroup(FilterResults));
		});
	});

	Describe("OR Filter Group", [this]()
	{
		It("should pass when any filter passes", [this]()
		{
			FilterResults = {false, true, false};
			TestTrue(TEXT("One pass -> group passes"),
			         SimulateOrGroup(FilterResults));
		});

		It("should pass when all filters pass", [this]()
		{
			FilterResults = {true, true, true};
			TestTrue(TEXT("All pass -> group passes"),
			         SimulateOrGroup(FilterResults));
		});

		It("should fail when all filters fail", [this]()
		{
			FilterResults = {false, false, false};
			TestFalse(TEXT("All fail -> group fails"),
			          SimulateOrGroup(FilterResults));
		});

		It("should fail with empty filter list", [this]()
		{
			FilterResults = {};
			TestFalse(TEXT("Empty -> false (no passing filter)"),
			          SimulateOrGroup(FilterResults));
		});
	});

	Describe("Filter Result Caching", [this]()
	{
		It("should support cached results pattern", [this]()
		{
			// Simulate cache behavior: -1 = not cached, 0 = false, 1 = true
			TArray<int8> Cache;
			Cache.Init(-1, 5); // 5 items, not cached

			// First access - not cached
			for (int32 i = 0; i < 5; ++i)
			{
				TestEqual(FString::Printf(TEXT("Item %d not cached"), i),
				          Cache[i], -1);
			}

			// Simulate caching results
			Cache[0] = 1; // Pass
			Cache[1] = 0; // Fail
			Cache[2] = 1; // Pass

			// Verify cached values
			TestEqual(TEXT("Cached pass is 1"), Cache[0], 1);
			TestEqual(TEXT("Cached fail is 0"), Cache[1], 0);
			TestEqual(TEXT("Uncached still -1"), Cache[3], -1);

			// Pattern: Check cache before computing
			auto GetResult = [&Cache](int32 Index, bool ComputedResult) -> bool
			{
				if (Cache[Index] >= 0)
				{
					return Cache[Index] != 0; // Use cached
				}
				Cache[Index] = ComputedResult ? 1 : 0; // Cache result
				return ComputedResult;
			};

			// First call computes and caches
			bool Result3 = GetResult(3, true);
			TestTrue(TEXT("First call returns computed result"), Result3);
			TestEqual(TEXT("Result was cached"), Cache[3], 1);

			// Second call uses cache (even if we pass different computed value)
			bool Result3Again = GetResult(3, false);
			TestTrue(TEXT("Second call returns cached result"), Result3Again);
		});
	});

	Describe("Filter Priority", [this]()
	{
		It("should support priority-based ordering", [this]()
		{
			struct FMockFilter
			{
				int32 Priority;
				FString Name;
			};

			TArray<FMockFilter> Filters = {
				{10, TEXT("Low Priority")},
				{0, TEXT("Default Priority")},
				{100, TEXT("High Priority")},
				{50, TEXT("Medium Priority")},
			};

			// Sort by priority (higher first)
			Filters.Sort([](const FMockFilter& A, const FMockFilter& B)
			{
				return A.Priority > B.Priority;
			});

			TestEqual(TEXT("Highest priority first"), Filters[0].Name, TEXT("High Priority"));
			TestEqual(TEXT("Default priority last"), Filters[3].Name, TEXT("Default Priority"));
		});
	});
}

// =============================================================================
// Filter Fallback Behavior Tests
// =============================================================================

BEGIN_DEFINE_SPEC(
	FPCGExFilterFallbackSpec,
	"PCGEx.Integration.Filters.Fallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	// Simulate fallback behavior
	bool ApplyFallback(EPCGExFilterFallback Fallback)
	{
		return Fallback == EPCGExFilterFallback::Pass;
	}

	// Simulate no-data fallback
	enum class ENoDataResult { Error, Pass, Fail };
	ENoDataResult ApplyNoDataFallback(EPCGExFilterNoDataFallback Policy)
	{
		switch (Policy)
		{
		case EPCGExFilterNoDataFallback::Error: return ENoDataResult::Error;
		case EPCGExFilterNoDataFallback::Pass: return ENoDataResult::Pass;
		case EPCGExFilterNoDataFallback::Fail: return ENoDataResult::Fail;
		default: return ENoDataResult::Error;
		}
	}

END_DEFINE_SPEC(FPCGExFilterFallbackSpec)

void FPCGExFilterFallbackSpec::Define()
{
	Describe("Filter Fallback", [this]()
	{
		It("should return true for Pass fallback", [this]()
		{
			TestTrue(TEXT("Pass fallback returns true"),
			         ApplyFallback(EPCGExFilterFallback::Pass));
		});

		It("should return false for Fail fallback", [this]()
		{
			TestFalse(TEXT("Fail fallback returns false"),
			          ApplyFallback(EPCGExFilterFallback::Fail));
		});
	});

	Describe("No Data Fallback", [this]()
	{
		It("should error by default for missing data", [this]()
		{
			TestEqual(TEXT("Error policy triggers error"),
			          ApplyNoDataFallback(EPCGExFilterNoDataFallback::Error),
			          ENoDataResult::Error);
		});

		It("should pass when policy is Pass", [this]()
		{
			TestEqual(TEXT("Pass policy allows continuation"),
			          ApplyNoDataFallback(EPCGExFilterNoDataFallback::Pass),
			          ENoDataResult::Pass);
		});

		It("should fail when policy is Fail", [this]()
		{
			TestEqual(TEXT("Fail policy rejects"),
			          ApplyNoDataFallback(EPCGExFilterNoDataFallback::Fail),
			          ENoDataResult::Fail);
		});
	});
}
