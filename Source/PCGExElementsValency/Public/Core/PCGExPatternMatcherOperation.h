// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"
#include "PCGExValencyPattern.h"
#include "PCGExValencyOrbitalCache.h"
#include "PCGPin.h"

#include "PCGExPatternMatcherOperation.generated.h"

/**
 * Overlap resolution strategy when multiple patterns match the same nodes.
 */
UENUM(BlueprintType)
enum class EPCGExPatternOverlapResolution : uint8
{
	WeightBased UMETA(ToolTip = "Use pattern weights for probabilistic selection"),
	LargestFirst UMETA(ToolTip = "Prefer patterns with more entries"),
	SmallestFirst UMETA(ToolTip = "Prefer patterns with fewer entries"),
	FirstDefined UMETA(ToolTip = "Use pattern definition order in BondingRules")
};

class UPCGBasePointData;

namespace PCGExData
{
	class FFacade;
	class FFacadePreloader;
	class FPointIO;
	template <typename T>
	class TBuffer;
}

namespace PCGExValency
{
	struct FValencyState;
}

namespace PCGExPatternMatcher
{
	/**
	 * Base class for matcher-specific shared data/allocations.
	 * Matchers that need access to point attributes should derive from this
	 * and override CreateAllocations() in their factory to populate it.
	 */
	struct PCGEXELEMENTSVALENCY_API FMatcherAllocations : TSharedFromThis<FMatcherAllocations>
	{
		virtual ~FMatcherAllocations() = default;
	};

	/**
	 * Result of a match operation.
	 */
	struct PCGEXELEMENTSVALENCY_API FMatchResult
	{
		/** All matches found */
		TArray<FPCGExValencyPatternMatch> Matches;

		/** Number of patterns that matched at least once */
		int32 PatternsMatched = 0;

		/** Total number of nodes annotated */
		int32 NodesAnnotated = 0;

		/** True if matching completed without errors */
		bool bSuccess = false;
	};

	//
	// === Custom Output Support (Extensibility) ===
	//
	// Matchers can request additional output pins and write data to them.
	// This is handled per-cluster, per-matcher during the Write phase.
	//

	/**
	 * Context for matcher output operations.
	 * Provides access to source data and output routing.
	 * Created by the host batch and passed to matchers during Write phase.
	 */
	struct PCGEXELEMENTSVALENCY_API FMatcherOutputContext
	{
		/** Source vertex facade (read-only access for copying) */
		TSharedPtr<PCGExData::FFacade> SourceVtxFacade;

		/** Source point data for transform access */
		const UPCGBasePointData* SourcePointData = nullptr;

		/**
		 * Additional outputs requested by this matcher's factory.
		 * Key = pin label declared by factory, Value = output point collection.
		 * Populated by host batch based on factory's GetOutputPinProperties().
		 */
		TMap<FName, TSharedPtr<PCGExData::FPointIO>> AdditionalOutputs;

		/** Check if an additional output exists */
		bool HasOutput(FName PinLabel) const { return AdditionalOutputs.Contains(PinLabel); }

		/** Get an additional output (nullptr if not declared) */
		TSharedPtr<PCGExData::FPointIO> GetOutput(FName PinLabel) const
		{
			const TSharedPtr<PCGExData::FPointIO>* Found = AdditionalOutputs.Find(PinLabel);
			return Found ? *Found : nullptr;
		}
	};
}

/**
 * Base class for pattern matcher operations.
 * Derive from this to create custom matching algorithms.
 *
 * Matchers receive compiled patterns, orbital cache, and module data,
 * then find matches and annotate them. Matchers MUST NOT modify cluster topology.
 *
 * Lifecycle:
 * 1. Factory::CreateOperation() creates the operation
 * 2. Initialize() receives shared state (patterns, cache, module data)
 * 3. Match() finds all matches (populates Matches array)
 * 4. Annotate() writes standard attributes to matched nodes
 * 5. WriteCustomOutput() (optional) writes to additional output pins
 */
class PCGEXELEMENTSVALENCY_API FPCGExPatternMatcherOperation : public FPCGExOperation
{
public:
	FPCGExPatternMatcherOperation() = default;
	virtual ~FPCGExPatternMatcherOperation() override = default;

	/**
	 * Initialize the matcher with pattern data and cluster state.
	 * @param InCompiledPatterns All compiled patterns from BondingRules
	 * @param InOrbitalCache Orbital cache for this cluster
	 * @param InModuleDataReader Reader for module data attribute
	 * @param InNumNodes Number of nodes in the cluster
	 * @param InClaimedNodes Set of already-claimed nodes (for exclusive matching)
	 * @param InSeed Random seed for deterministic selection
	 * @param InAllocations Optional matcher-specific allocations
	 */
	virtual void Initialize(
		const FPCGExValencyPatternSetCompiled* InCompiledPatterns,
		const PCGExValency::FOrbitalCache* InOrbitalCache,
		const TSharedPtr<PCGExData::TBuffer<int64>>& InModuleDataReader,
		int32 InNumNodes,
		TSet<int32>* InClaimedNodes,
		int32 InSeed,
		const TSharedPtr<PCGExPatternMatcher::FMatcherAllocations>& InAllocations = nullptr);

	/**
	 * Find all matches. Must populate Matches array.
	 * @return Result with match statistics
	 */
	virtual PCGExPatternMatcher::FMatchResult Match() PCGEX_NOT_IMPLEMENTED_RET(Match, PCGExPatternMatcher::FMatchResult());

	/**
	 * Write standard annotations to matched nodes.
	 * Base implementation writes PatternName and MatchIndex attributes.
	 * @param PatternNameWriter Writer for pattern name attribute
	 * @param MatchIndexWriter Writer for match index attribute
	 */
	virtual void Annotate(
		const TSharedPtr<PCGExData::TBuffer<FName>>& PatternNameWriter,
		const TSharedPtr<PCGExData::TBuffer<int32>>& MatchIndexWriter);

	/**
	 * Write to additional output pins (optional).
	 * Override in derived classes that declare custom output pins.
	 * Called after Annotate() during the Write phase.
	 * @param OutputContext Context with source data and output collections
	 */
	virtual void WriteCustomOutput(PCGExPatternMatcher::FMatcherOutputContext& OutputContext) {}

	/** Get the matches found by this operation */
	const TArray<FPCGExValencyPatternMatch>& GetMatches() const { return Matches; }
	TArray<FPCGExValencyPatternMatch>& GetMatches() { return Matches; }

	/** Whether this matcher claims exclusive ownership of matched nodes */
	bool bExclusive = true;

	/**
	 * Pattern filter function (set by factory via InitOperation).
	 * Returns true if the pattern at the given index should be considered for matching.
	 */
	TFunction<bool(int32 PatternIndex, const FPCGExValencyPatternSetCompiled* Patterns)> PatternFilter;

protected:
	/** Compiled patterns (owned externally by BondingRules) */
	const FPCGExValencyPatternSetCompiled* CompiledPatterns = nullptr;

	/** Orbital cache (owned externally by processor) */
	const PCGExValency::FOrbitalCache* OrbitalCache = nullptr;

	/** Module data reader (owned externally by batch) */
	TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataReader;

	/** Number of nodes in cluster */
	int32 NumNodes = 0;

	/** Claimed nodes set (shared across matchers for exclusivity) */
	TSet<int32>* ClaimedNodes = nullptr;

	/** Matcher-specific allocations (optional) */
	TSharedPtr<PCGExPatternMatcher::FMatcherAllocations> Allocations;

	/** Random stream for deterministic selection */
	FRandomStream RandomStream;

	/** Matches found by this operation */
	TArray<FPCGExValencyPatternMatch> Matches;

	// === Utility methods for derived classes ===

	/** Get module index for a node */
	int32 GetModuleIndex(int32 NodeIndex) const;

	/** Get neighbor at orbital for a node (-1 if none) */
	FORCEINLINE int32 GetNeighborAtOrbital(int32 NodeIndex, int32 OrbitalIndex) const
	{
		return OrbitalCache ? OrbitalCache->GetNeighborAtOrbital(NodeIndex, OrbitalIndex) : -1;
	}

	/** Get orbital mask for a node */
	FORCEINLINE int64 GetOrbitalMask(int32 NodeIndex) const
	{
		return OrbitalCache ? OrbitalCache->GetOrbitalMask(NodeIndex) : 0;
	}

	/** Check if a node is already claimed */
	FORCEINLINE bool IsNodeClaimed(int32 NodeIndex) const
	{
		return ClaimedNodes && ClaimedNodes->Contains(NodeIndex);
	}

	/** Claim a node (for exclusive matchers) */
	FORCEINLINE void ClaimNode(int32 NodeIndex)
	{
		if (ClaimedNodes) { ClaimedNodes->Add(NodeIndex); }
	}
};

/**
 * Base factory for creating pattern matcher operations.
 * Subclass this and override CreateOperation() to provide custom matchers.
 */
UCLASS(Abstract)
class PCGEXELEMENTSVALENCY_API UPCGExPatternMatcherFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	/** Create the matcher operation instance */
	virtual TSharedPtr<FPCGExPatternMatcherOperation> CreateOperation() const PCGEX_NOT_IMPLEMENTED_RET(CreateOperation, nullptr);

	/**
	 * Register buffer dependencies for this matcher.
	 * Override to declare attributes that need preloading.
	 */
	virtual void RegisterPrimaryBuffersDependencies(
		FPCGExContext* InContext,
		PCGExData::FFacadePreloader& FacadePreloader) const override;

	/**
	 * Create matcher-specific allocations from the vertex facade.
	 * Override to read attributes and build data structures needed by the matcher.
	 * @param VtxFacade The vertex data facade with preloaded buffers
	 * @return Allocations object, or nullptr if matcher doesn't need allocations
	 */
	virtual TSharedPtr<PCGExPatternMatcher::FMatcherAllocations> CreateAllocations(
		const TSharedRef<PCGExData::FFacade>& VtxFacade) const;

	/**
	 * Declare additional output pins for this matcher.
	 * Override to add custom output pins (e.g., Fork matcher adds "Forked" pin).
	 * @return Array of pin properties to add to the host node
	 */
	virtual TArray<FPCGPinProperties> GetOutputPinProperties() const { return {}; }

	// === Pattern Selection ===

	/**
	 * Filter patterns by actor tags (from pattern root cage).
	 * Empty = no tag filtering.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pattern Selection", meta=(PCG_Overridable))
	TArray<FName> RequiredTags;

	/**
	 * Exclude patterns with these tags.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pattern Selection", meta=(PCG_Overridable))
	TArray<FName> ExcludedTags;

	/**
	 * Match only specific patterns by name.
	 * Empty = match all patterns (subject to tag filters).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pattern Selection", meta=(PCG_Overridable))
	TArray<FName> PatternNames;

	/**
	 * Whether this matcher claims exclusive ownership of matched nodes.
	 * If true, matched nodes won't be matched by subsequent matchers.
	 * Note: Pattern's own bExclusive setting may also affect this.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bExclusive = true;

	/**
	 * Check if a pattern passes the selection filters.
	 * @param Pattern The compiled pattern to check
	 * @param PatternTags Tags from the pattern's source cage (actor tags)
	 * @return True if pattern should be considered by this matcher
	 */
	bool PassesPatternFilter(const FPCGExValencyPatternCompiled& Pattern, const TArray<FName>& PatternTags) const;

protected:
	/**
	 * Initialize common operation settings from factory properties.
	 * Call this from derived CreateOperation() implementations.
	 * @param Operation The operation to initialize
	 */
	void InitOperation(const TSharedPtr<FPCGExPatternMatcherOperation>& Operation) const;
};
