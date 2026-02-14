// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * SharedTag Match Rule Unit Tests
 *
 * Tests the SharedTag match rule operation via factory + base-class interface:
 * - Specific mode: exact tag matching (raw and value tags)
 * - AnyShared mode: any shared tag between target and candidate
 * - AllShared mode: all candidate tags present in target
 * - Value matching variants
 * - Invert flag behavior
 *
 * Note: FPCGExMatchSharedTag/Config are not DLL-exported, so tests go through
 * UPCGExMatchSharedTagFactory::CreateOperation → FPCGExMatchRuleOperation (exported).
 *
 * Test naming convention: PCGEx.Unit.Matching.SharedTag.<Mode>.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Matching/PCGExMatchSharedTag.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExTaggedData.h"
#include "Data/PCGExPointElements.h"
#include "Fixtures/PCGExTestContext.h"

namespace PCGExSharedTagTestHelpers
{
	/**
	 * Run a single SharedTag test with one source and one candidate.
	 * Creates factory, operation (via exported base class), point data, tags, and evaluates.
	 */
	static bool RunTest(
		const EPCGExTagMatchMode Mode,
		const FString& TagName,
		const bool bDoValueMatch,
		const bool bMatchTagValues,
		const bool bInvert,
		const TSet<FString>& InSourceTags,
		const TSet<FString>& InCandidateTags)
	{
		// Create factory and configure through the UObject member (avoids stack-instantiating non-exported config)
		UPCGExMatchSharedTagFactory* Factory = NewObject<UPCGExMatchSharedTagFactory>(GetTransientPackage(), NAME_None, RF_Transient);
		Factory->Config.Mode = Mode;
		Factory->Config.TagNameInput = EPCGExInputValueType::Constant;
		Factory->Config.TagName = TagName;
		Factory->Config.bDoValueMatch = bDoValueMatch;
		Factory->Config.bMatchTagValues = bMatchTagValues;
		Factory->Config.bInvert = bInvert;
		Factory->BaseConfig = Factory->Config;

		// CreateOperation handles Config copy + Init() internally, returns exported base class
		const TSharedPtr<FPCGExMatchRuleOperation> Op = Factory->CreateOperation(nullptr);
		if (!Op) { return false; }

		UPCGBasePointData* SrcData = PCGExTest::FSimplePointDataFactory::CreateSequential(1);
		UPCGBasePointData* CndData = PCGExTest::FSimplePointDataFactory::CreateSequential(1);

		TSharedPtr<PCGExData::FTags> SrcTags = MakeShared<PCGExData::FTags>(InSourceTags);
		TSharedPtr<PCGExData::FTags> CndTags = MakeShared<PCGExData::FTags>(InCandidateTags);

		auto Sources = MakeShared<TArray<FPCGExTaggedData>>();
		Sources->Add(FPCGExTaggedData(SrcData, 0, SrcTags, nullptr));

		Op->PrepareForMatchableSources(nullptr, Sources);

		FPCGExTaggedData Candidate(CndData, 0, CndTags, nullptr);
		PCGExData::FConstPoint TargetPoint(SrcData, 0, 0);
		PCGExMatching::FScope Scope(1, true);

		return Op->Test(TargetPoint, Candidate, Scope);
	}

	/** Convenience overload for tests that only need Mode + TagName (common Specific case). */
	static bool RunSpecificTest(
		const FString& TagName,
		const bool bDoValueMatch,
		const bool bInvert,
		const TSet<FString>& InSourceTags,
		const TSet<FString>& InCandidateTags)
	{
		return RunTest(EPCGExTagMatchMode::Specific, TagName, bDoValueMatch, false, bInvert, InSourceTags, InCandidateTags);
	}
}

// =============================================================================
// Specific Mode - Raw Tags
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificMatchTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.Match",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("TagA"), false, false,
		{TEXT("TagA"), TEXT("TagB")},
		{TEXT("TagA"), TEXT("TagC")});

	TestTrue(TEXT("Both have TagA -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificNoMatchTargetTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.NoMatch.MissingFromTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificNoMatchTargetTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("TagA"), false, false,
		{TEXT("TagB"), TEXT("TagC")},
		{TEXT("TagA")});

	TestFalse(TEXT("Target lacks TagA -> no match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificNoMatchCandidateTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.NoMatch.MissingFromCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificNoMatchCandidateTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("TagA"), false, false,
		{TEXT("TagA")},
		{TEXT("TagB"), TEXT("TagC")});

	TestFalse(TEXT("Candidate lacks TagA -> no match"), bResult);
	return true;
}

// =============================================================================
// Specific Mode - Value Tags
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificValueTagNameOnlyTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.ValueTag.NameOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificValueTagNameOnlyTest::RunTest(const FString& Parameters)
{
	// Both have "Mod" as a value tag with different values.
	// bDoValueMatch=false -> match based on tag name presence only.
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("Mod"), false, false,
		{TEXT("Mod:42")},
		{TEXT("Mod:99")});

	TestTrue(TEXT("Both have Mod value tag, no value check -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificValueTagMixedTypeTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.ValueTag.MixedType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificValueTagMixedTypeTest::RunTest(const FString& Parameters)
{
	// Source has "Mod" as a value tag, candidate has "Mod" as a raw tag.
	// One has a value, the other doesn't -> no match.
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("Mod"), false, false,
		{TEXT("Mod:42")},
		{TEXT("Mod")});

	TestFalse(TEXT("One value tag + one raw tag -> no match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificValueMatchSameTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.ValueMatch.Same",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificValueMatchSameTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("Score"), true, false,
		{TEXT("Score:100")},
		{TEXT("Score:100")});

	TestTrue(TEXT("Same value -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagSpecificValueMatchDifferentTest,
	"PCGEx.Unit.Matching.SharedTag.Specific.ValueMatch.Different",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagSpecificValueMatchDifferentTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("Score"), true, false,
		{TEXT("Score:100")},
		{TEXT("Score:200")});

	TestFalse(TEXT("Different value -> no match"), bResult);
	return true;
}

// =============================================================================
// AnyShared Mode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAnySharedMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AnyShared.Match",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAnySharedMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AnyShared, TEXT(""), false, false, false,
		{TEXT("TagA"), TEXT("TagB")},
		{TEXT("TagB"), TEXT("TagC")});

	TestTrue(TEXT("Share TagB -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAnySharedNoMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AnyShared.NoMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAnySharedNoMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AnyShared, TEXT(""), false, false, false,
		{TEXT("TagA"), TEXT("TagB")},
		{TEXT("TagC"), TEXT("TagD")});

	TestFalse(TEXT("No shared tags -> no match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAnySharedValuesMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AnyShared.MatchTagValues.Match",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAnySharedValuesMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AnyShared, TEXT(""), false, true, false,
		{TEXT("Color:Red")},
		{TEXT("Color:Red")});

	TestTrue(TEXT("Same value tag shared -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAnySharedValuesNoMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AnyShared.MatchTagValues.NoMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAnySharedValuesNoMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AnyShared, TEXT(""), false, true, false,
		{TEXT("Color:Red")},
		{TEXT("Color:Blue")});

	TestFalse(TEXT("Different values -> no match with value check"), bResult);
	return true;
}

// =============================================================================
// AllShared Mode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAllSharedMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AllShared.Match",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAllSharedMatchTest::RunTest(const FString& Parameters)
{
	// Target has all candidate tags plus extras
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AllShared, TEXT(""), false, false, false,
		{TEXT("TagA"), TEXT("TagB"), TEXT("TagC")},
		{TEXT("TagA"), TEXT("TagB")});

	TestTrue(TEXT("All candidate tags in target -> match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAllSharedNoMatchTest,
	"PCGEx.Unit.Matching.SharedTag.AllShared.NoMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAllSharedNoMatchTest::RunTest(const FString& Parameters)
{
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AllShared, TEXT(""), false, false, false,
		{TEXT("TagA"), TEXT("TagC")},
		{TEXT("TagA"), TEXT("TagB")});

	TestFalse(TEXT("Target missing TagB -> no match"), bResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagAllSharedEmptyCandidateTest,
	"PCGEx.Unit.Matching.SharedTag.AllShared.EmptyCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagAllSharedEmptyCandidateTest::RunTest(const FString& Parameters)
{
	// Empty candidate: vacuously true
	const bool bResult = PCGExSharedTagTestHelpers::RunTest(
		EPCGExTagMatchMode::AllShared, TEXT(""), false, false, false,
		{TEXT("TagA")},
		{});

	TestTrue(TEXT("Empty candidate -> always matches"), bResult);
	return true;
}

// =============================================================================
// Invert
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExSharedTagInvertTest,
	"PCGEx.Unit.Matching.SharedTag.Invert.FlipsResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExSharedTagInvertTest::RunTest(const FString& Parameters)
{
	// Would normally match, but inverted
	const bool bMatchInverted = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("TagA"), false, true,
		{TEXT("TagA")},
		{TEXT("TagA")});

	TestFalse(TEXT("Invert flips match -> false"), bMatchInverted);

	// Would normally not match, inverted becomes match
	const bool bNoMatchInverted = PCGExSharedTagTestHelpers::RunSpecificTest(
		TEXT("TagA"), false, true,
		{TEXT("TagA")},
		{TEXT("TagB")});

	TestTrue(TEXT("Invert flips no-match -> true"), bNoMatchInverted);

	return true;
}
