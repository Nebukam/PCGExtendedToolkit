// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPatternMatcherOperation.h"

#include "PCGExDefaultPatternMatcher.generated.h"

/**
 * Default pattern matcher operation.
 * Performs full subgraph isomorphism matching using DFS with backtracking.
 * Ports the existing matching algorithm from PCGExValencyPatternReplacement::FProcessor.
 */
class PCGEXELEMENTSVALENCY_API FPCGExDefaultPatternMatcherOperation : public FPCGExPatternMatcherOperation
{
public:
	FPCGExDefaultPatternMatcherOperation() = default;
	virtual ~FPCGExDefaultPatternMatcherOperation() override = default;

	/** Overlap resolution strategy */
	EPCGExPatternOverlapResolution OverlapResolution = EPCGExPatternOverlapResolution::WeightBased;

	//~ Begin FPCGExPatternMatcherOperation Interface
	virtual PCGExPatternMatcher::FMatchResult Match() override;
	//~ End FPCGExPatternMatcherOperation Interface

protected:
	/** Find all matches for a single pattern, respecting MaxMatches limit */
	void FindMatchesForPattern(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern);

	/** Try to match a pattern starting from a specific node */
	bool TryMatchPatternFromNode(
		int32 PatternIndex,
		const FPCGExValencyPatternCompiled& Pattern,
		int32 StartNodeIndex,
		FPCGExValencyPatternMatch& OutMatch);

	/** Recursive DFS matching helper */
	bool MatchEntryRecursive(
		const FPCGExValencyPatternCompiled& Pattern,
		int32 EntryIndex,
		TArray<int32>& EntryToNode,
		TSet<int32>& UsedNodes);

	/** Resolve overlapping matches based on OverlapResolution strategy */
	void ResolveOverlaps();

	/** Claim nodes for exclusive matches (after overlap resolution) */
	void ClaimMatchedNodes();

	/** Validate MinMatches constraints and update result */
	void ValidateMinMatches(PCGExPatternMatcher::FMatchResult& OutResult);

	/** Track match counts per pattern (PatternIndex -> count) */
	TMap<int32, int32> PatternMatchCounts;
};

/**
 * Default pattern matcher factory.
 * Creates subgraph isomorphism matchers that use compiled patterns from BondingRules.
 */
UCLASS(DisplayName = "Default Pattern Matcher", meta=(ToolTip = "Default subgraph isomorphism pattern matcher using DFS with backtracking.", PCGExNodeLibraryDoc="valency/matchers/default"))
class PCGEXELEMENTSVALENCY_API UPCGExDefaultPatternMatcher : public UPCGExPatternMatcherFactory
{
	GENERATED_BODY()

public:
	//~ Begin UPCGExPatternMatcherFactory Interface
	virtual TSharedPtr<FPCGExPatternMatcherOperation> CreateOperation() const override;
	//~ End UPCGExPatternMatcherFactory Interface

	/** How to resolve overlapping pattern matches */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPatternOverlapResolution OverlapResolution = EPCGExPatternOverlapResolution::WeightBased;
};
