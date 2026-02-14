// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMatching::FDataMatcher Unit & Integration Tests
 *
 * Tests the FDataMatcher orchestration layer:
 * - Init with factories
 * - Disabled / All / Any match modes
 * - Data-level Test(UPCGData*, ...) and point-level Test(FConstPoint, ...)
 * - BuildPerPointExclude
 * - GetMatchingSourcesIndices with multiple sources
 *
 * Uses FScopedTestContext for facade creation and UPCGExMatchSharedTagFactory
 * for match rules (tag matching is the simplest to set up without attributes).
 *
 * Test naming convention: PCGEx.Unit.Matching.DataMatcher.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Details/PCGExMatchingDetails.h"
#include "Matching/PCGExMatchSharedTag.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExTaggedData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExPointElements.h"
#include "Fixtures/PCGExTestContext.h"

namespace PCGExDataMatcherTestHelpers
{
	/** Create a SharedTag factory configured for Specific + Constant mode. */
	static UPCGExMatchSharedTagFactory* MakeTagFactory(const FString& TagName)
	{
		UPCGExMatchSharedTagFactory* Factory = NewObject<UPCGExMatchSharedTagFactory>(GetTransientPackage(), NAME_None, RF_Transient);
		Factory->Config.Mode = EPCGExTagMatchMode::Specific;
		Factory->Config.TagNameInput = EPCGExInputValueType::Constant;
		Factory->Config.TagName = TagName;
		Factory->BaseConfig = Factory->Config;
		return Factory;
	}

	/** Create a candidate FPCGExTaggedData backed by simple point data with the given tags. */
	struct FTaggedCandidate
	{
		UPCGBasePointData* Data;
		TSharedPtr<PCGExData::FTags> Tags;
		FPCGExTaggedData TaggedData;

		FTaggedCandidate(const TSet<FString>& InTags)
		{
			Data = PCGExTest::FSimplePointDataFactory::CreateSequential(1);
			Tags = MakeShared<PCGExData::FTags>(InTags);
			TaggedData = FPCGExTaggedData(Data, 0, Tags, nullptr);
		}
	};
}

// =============================================================================
// Init
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherInitTest,
	"PCGEx.Unit.Matching.DataMatcher.Init.WithFactories",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherInitTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(5);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);

	const bool bInitOk = Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);
	TestTrue(TEXT("Init succeeds with valid factory and source"), bInitOk);

	return true;
}

// =============================================================================
// Disabled Mode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherDisabledTest,
	"PCGEx.Unit.Matching.DataMatcher.Disabled.AlwaysTrue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherDisabledTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(3);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::Disabled);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Non-matching candidate should still return true in Disabled mode
	FTaggedCandidate Candidate({TEXT("Unrelated")});
	PCGExMatching::FScope Scope(10, true);

	const bool bResult = Matcher->Test(Facade->Source->GetIn(), Candidate.TaggedData, Scope);
	TestTrue(TEXT("Disabled mode always returns true"), bResult);

	return true;
}

// =============================================================================
// All Mode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherAllMatchTest,
	"PCGEx.Unit.Matching.DataMatcher.All.AllRulesPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherAllMatchTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(3);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA"), TEXT("TagB")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));
	Factories.Add(MakeTagFactory(TEXT("TagB")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Candidate has both tags
	FTaggedCandidate Candidate({TEXT("TagA"), TEXT("TagB")});
	PCGExMatching::FScope Scope(10, true);

	const bool bResult = Matcher->Test(Facade->Source->GetIn(), Candidate.TaggedData, Scope);
	TestTrue(TEXT("All mode: both rules pass -> match"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherAllNoMatchTest,
	"PCGEx.Unit.Matching.DataMatcher.All.OneRuleFails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherAllNoMatchTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(3);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA"), TEXT("TagB")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));
	Factories.Add(MakeTagFactory(TEXT("TagB")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Candidate only has TagA, missing TagB
	FTaggedCandidate Candidate({TEXT("TagA")});
	PCGExMatching::FScope Scope(10, true);

	const bool bResult = Matcher->Test(Facade->Source->GetIn(), Candidate.TaggedData, Scope);
	TestFalse(TEXT("All mode: one rule fails -> no match"), bResult);

	return true;
}

// =============================================================================
// Any Mode
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherAnyOneMatchTest,
	"PCGEx.Unit.Matching.DataMatcher.Any.OneRulePasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherAnyOneMatchTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(3);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA"), TEXT("TagB")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));
	Factories.Add(MakeTagFactory(TEXT("TagB")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::Any);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Candidate has TagA but not TagB - one rule passes
	FTaggedCandidate Candidate({TEXT("TagA")});
	PCGExMatching::FScope Scope(10, true);

	const bool bResult = Matcher->Test(Facade->Source->GetIn(), Candidate.TaggedData, Scope);
	TestTrue(TEXT("Any mode: one rule passes -> match"), bResult);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherAnyNoneMatchTest,
	"PCGEx.Unit.Matching.DataMatcher.Any.NoRulesPass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherAnyNoneMatchTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(3);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA"), TEXT("TagB")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));
	Factories.Add(MakeTagFactory(TEXT("TagB")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::Any);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Candidate has neither tag
	FTaggedCandidate Candidate({TEXT("TagC"), TEXT("TagD")});
	PCGExMatching::FScope Scope(10, true);

	const bool bResult = Matcher->Test(Facade->Source->GetIn(), Candidate.TaggedData, Scope);
	TestFalse(TEXT("Any mode: no rules pass -> no match"), bResult);

	return true;
}

// =============================================================================
// Point-Level Test
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherPointLevelTest,
	"PCGEx.Unit.Matching.DataMatcher.Test.PointLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherPointLevelTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto Facade = Ctx->CreateFacade(5);
	Facade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{Facade}, false);

	// Test using FConstPoint (point-level) instead of UPCGData* (data-level)
	PCGExData::FConstPoint Point(Facade->Source->GetIn(), 2, 0); // point index 2, IO=0

	FTaggedCandidate MatchCandidate({TEXT("TagA")});
	PCGExMatching::FScope Scope1(10, true);
	const bool bMatch = Matcher->Test(Point, MatchCandidate.TaggedData, Scope1);
	TestTrue(TEXT("Point-level: matching candidate -> true"), bMatch);

	FTaggedCandidate NoMatchCandidate({TEXT("TagB")});
	PCGExMatching::FScope Scope2(10, true);
	const bool bNoMatch = Matcher->Test(Point, NoMatchCandidate.TaggedData, Scope2);
	TestFalse(TEXT("Point-level: non-matching candidate -> false"), bNoMatch);

	return true;
}

// =============================================================================
// BuildPerPointExclude
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherBuildExcludeTest,
	"PCGEx.Unit.Matching.DataMatcher.BuildPerPointExclude.Correct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherBuildExcludeTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	// Source (input) facade with TagA
	auto InputFacade = Ctx->CreateFacade(5);
	InputFacade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{InputFacade}, false);

	// Two candidates: one matching, one not
	FTaggedCandidate MatchCand({TEXT("TagA")});
	FTaggedCandidate NoMatchCand({TEXT("TagB")});

	TArray<FPCGExTaggedData> Candidates;
	Candidates.Add(MatchCand.TaggedData);
	Candidates.Add(NoMatchCand.TaggedData);

	TSet<const UPCGData*> Exclude;
	PCGExData::FConstPoint TestPoint = InputFacade->Source->GetInPoint(2);

	const bool bAnyMatch = Matcher->BuildPerPointExclude(TestPoint, Candidates, Exclude);

	TestTrue(TEXT("At least one candidate matched"), bAnyMatch);
	TestEqual(TEXT("One candidate excluded"), Exclude.Num(), 1);
	TestTrue(TEXT("Non-matching candidate is excluded"), Exclude.Contains(NoMatchCand.Data));
	TestFalse(TEXT("Matching candidate is not excluded"), Exclude.Contains(MatchCand.Data));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherBuildExcludeNoneTest,
	"PCGEx.Unit.Matching.DataMatcher.BuildPerPointExclude.NoneMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherBuildExcludeNoneTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	auto InputFacade = Ctx->CreateFacade(3);
	InputFacade->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);
	Matcher->Init(Factories, TArray<TSharedPtr<PCGExData::FFacade>>{InputFacade}, false);

	// All candidates lack the required tag
	FTaggedCandidate Cand1({TEXT("TagB")});
	FTaggedCandidate Cand2({TEXT("TagC")});

	TArray<FPCGExTaggedData> Candidates;
	Candidates.Add(Cand1.TaggedData);
	Candidates.Add(Cand2.TaggedData);

	TSet<const UPCGData*> Exclude;
	PCGExData::FConstPoint TestPoint = InputFacade->Source->GetInPoint(0);

	const bool bAnyMatch = Matcher->BuildPerPointExclude(TestPoint, Candidates, Exclude);

	TestFalse(TEXT("No candidates matched"), bAnyMatch);
	TestEqual(TEXT("All candidates excluded"), Exclude.Num(), 2);

	return true;
}

// =============================================================================
// GetMatchingSourcesIndices
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExDataMatcherSourceIndicesTest,
	"PCGEx.Unit.Matching.DataMatcher.GetMatchingSourcesIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExDataMatcherSourceIndicesTest::RunTest(const FString& Parameters)
{
	using namespace PCGExDataMatcherTestHelpers;

	PCGExTest::FScopedTestContext Ctx;
	if (!Ctx.IsValid()) { return false; }

	// Three sources: [0]=TagA, [1]=TagB, [2]=TagA
	auto Facade0 = Ctx->CreateFacade(3);
	Facade0->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	auto Facade1 = Ctx->CreateFacade(3);
	Facade1->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagB")});

	auto Facade2 = Ctx->CreateFacade(3);
	Facade2->Source->Tags = MakeShared<PCGExData::FTags>(TSet<FString>{TEXT("TagA")});

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> Factories;
	Factories.Add(MakeTagFactory(TEXT("TagA")));

	auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	FPCGExMatchingDetails Details(EPCGExMapMatchMode::All);
	Matcher->SetDetails(&Details);

	TArray<TSharedPtr<PCGExData::FFacade>> Sources;
	Sources.Add(Facade0);
	Sources.Add(Facade1);
	Sources.Add(Facade2);
	Matcher->Init(Factories, Sources, false);

	// Candidate has TagA
	FTaggedCandidate Candidate({TEXT("TagA")});

	TArray<int32> Matches;
	PCGExMatching::FScope Scope(10, true);
	Matcher->GetMatchingSourcesIndices(Candidate.TaggedData, Scope, Matches);

	TestEqual(TEXT("Two sources match"), Matches.Num(), 2);
	TestTrue(TEXT("Source 0 matches"), Matches.Contains(0));
	TestFalse(TEXT("Source 1 does not match"), Matches.Contains(1));
	TestTrue(TEXT("Source 2 matches"), Matches.Contains(2));

	return true;
}
