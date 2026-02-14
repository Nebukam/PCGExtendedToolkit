// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGExMatching::FScope Unit Tests
 *
 * Tests the atomic-safe match scope tracking struct:
 * - Construction (default, with candidates, unlimited)
 * - RegisterMatch increments counter
 * - GetCounter / GetNumCandidates accessors
 * - IsValid / Invalidate
 *
 * Test naming convention: PCGEx.Unit.Matching.Scope.<TestCase>
 */

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExDataMatcher.h"

// =============================================================================
// Default Construction
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeDefaultConstructionTest,
	"PCGEx.Unit.Matching.Scope.DefaultConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeDefaultConstructionTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope;

	TestEqual(TEXT("Default NumCandidates is 0"), Scope.GetNumCandidates(), 0);
	TestEqual(TEXT("Default Counter is 0"), Scope.GetCounter(), 0);
	TestTrue(TEXT("Default scope is valid"), Scope.IsValid());

	return true;
}

// =============================================================================
// Construction with Candidates
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeWithCandidatesTest,
	"PCGEx.Unit.Matching.Scope.WithCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeWithCandidatesTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(5);

	TestEqual(TEXT("NumCandidates is 5"), Scope.GetNumCandidates(), 5);
	TestEqual(TEXT("Counter starts at 0"), Scope.GetCounter(), 0);
	TestTrue(TEXT("Scope is valid"), Scope.IsValid());

	return true;
}

// =============================================================================
// Unlimited Scope
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeUnlimitedTest,
	"PCGEx.Unit.Matching.Scope.Unlimited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeUnlimitedTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(3, true);

	TestEqual(TEXT("NumCandidates is 3"), Scope.GetNumCandidates(), 3);
	TestTrue(TEXT("Unlimited scope is valid"), Scope.IsValid());

	// Counter starts at -MAX_int32 for unlimited scopes
	const int32 InitialCounter = Scope.GetCounter();
	TestTrue(TEXT("Unlimited counter starts very negative"), InitialCounter < 0);

	return true;
}

// =============================================================================
// RegisterMatch
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeRegisterMatchTest,
	"PCGEx.Unit.Matching.Scope.RegisterMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeRegisterMatchTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(10);

	TestEqual(TEXT("Counter starts at 0"), Scope.GetCounter(), 0);

	Scope.RegisterMatch();
	TestEqual(TEXT("Counter is 1 after one match"), Scope.GetCounter(), 1);

	Scope.RegisterMatch();
	Scope.RegisterMatch();
	TestEqual(TEXT("Counter is 3 after three matches"), Scope.GetCounter(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeRegisterMatchUnlimitedTest,
	"PCGEx.Unit.Matching.Scope.RegisterMatch.Unlimited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeRegisterMatchUnlimitedTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(1, true);

	const int32 InitialCounter = Scope.GetCounter();

	Scope.RegisterMatch();
	TestEqual(TEXT("Counter incremented by 1"), Scope.GetCounter(), InitialCounter + 1);

	// Even after many matches, unlimited scope stays valid
	for (int32 i = 0; i < 100; i++) { Scope.RegisterMatch(); }
	TestTrue(TEXT("Unlimited scope stays valid after many matches"), Scope.IsValid());

	return true;
}

// =============================================================================
// Invalidate
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeInvalidateTest,
	"PCGEx.Unit.Matching.Scope.Invalidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeInvalidateTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(5);

	TestTrue(TEXT("Scope starts valid"), Scope.IsValid());

	Scope.Invalidate();
	TestFalse(TEXT("Scope is invalid after Invalidate()"), Scope.IsValid());

	// Counter should be unaffected
	TestEqual(TEXT("Counter unchanged after invalidation"), Scope.GetCounter(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeInvalidateDoesNotResetTest,
	"PCGEx.Unit.Matching.Scope.Invalidate.Permanent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeInvalidateDoesNotResetTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(5);

	Scope.RegisterMatch();
	Scope.Invalidate();

	// RegisterMatch still increments counter even on invalid scope
	Scope.RegisterMatch();
	TestEqual(TEXT("Counter still increments"), Scope.GetCounter(), 2);
	TestFalse(TEXT("Scope stays invalid"), Scope.IsValid());

	return true;
}

// =============================================================================
// Zero Candidates
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeZeroCandidatesTest,
	"PCGEx.Unit.Matching.Scope.ZeroCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeZeroCandidatesTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(0);

	TestEqual(TEXT("NumCandidates is 0"), Scope.GetNumCandidates(), 0);
	TestTrue(TEXT("Zero-candidate scope is valid"), Scope.IsValid());

	return true;
}

// =============================================================================
// Large Candidate Count
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExScopeLargeCandidateCountTest,
	"PCGEx.Unit.Matching.Scope.LargeCandidateCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExScopeLargeCandidateCountTest::RunTest(const FString& Parameters)
{
	PCGExMatching::FScope Scope(100000);

	TestEqual(TEXT("NumCandidates is 100000"), Scope.GetNumCandidates(), 100000);
	TestTrue(TEXT("Large scope is valid"), Scope.IsValid());

	for (int32 i = 0; i < 1000; i++) { Scope.RegisterMatch(); }
	TestEqual(TEXT("Counter is 1000"), Scope.GetCounter(), 1000);

	return true;
}
