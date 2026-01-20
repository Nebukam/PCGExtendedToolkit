// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyProcessor.h"
#include "Core/PCGExValencyPattern.h"

#include "PCGExValencyPatternReplacement.generated.h"

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

/**
 * Valency Pattern Replacement - Detects and transforms patterns in solved clusters.
 * Reads solved module indices and matches against compiled patterns from BondingRules.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "wfc wave function collapse valency pattern replacement", PCGExNodeLibraryDoc="valency/valency-pattern-replacement"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyPatternReplacementSettings : public UPCGExValencyProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyPatternReplacement, "Valency : Patterns", "Detects patterns in solved clusters and transforms matched points.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	// This node requires BondingRules (which provides OrbitalSet)
	virtual bool WantsOrbitalSet() const override { return true; }
	virtual bool WantsBondingRules() const override { return true; }

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** How to resolve overlapping pattern matches */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Matching", meta=(PCG_Overridable))
	EPCGExPatternOverlapResolution OverlapResolution = EPCGExPatternOverlapResolution::WeightBased;

	/** If enabled, output matched points to a secondary pin (for Remove/Fork strategies) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputMatchedPoints = true;

	/** Attribute name for the pattern name annotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName PatternNameAttributeName = FName("PatternName");

	/** Attribute name for the pattern match index (which occurrence of the pattern this point belongs to) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FName PatternMatchIndexAttributeName = FName("PatternMatchIndex");

	/** Suppress warnings about no patterns in bonding rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietNoPatterns = false;
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternReplacementContext final : FPCGExValencyProcessorContext
{
	friend class FPCGExValencyPatternReplacementElement;

	/** Compiled patterns (reference from BondingRules) */
	const FPCGExValencyPatternSetCompiled* CompiledPatterns = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyPatternReplacementElement final : public FPCGExValencyProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyPatternReplacement)

	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValencyPatternReplacement
{
	class FBatch;

	class FProcessor final : public PCGExValencyMT::TProcessor<FPCGExValencyPatternReplacementContext, UPCGExValencyPatternReplacementSettings>
	{
		friend class FBatch;

	protected:
		/** Module data reader/writer (packed int64 from Staging output) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataReader;
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataWriter;

		/** All matches found in this cluster */
		TArray<FPCGExValencyPatternMatch> AllMatches;

		/** Claimed node indices (for exclusive patterns) */
		TSet<int32> ClaimedNodes;

		/** Node indices to remove from main output (Remove/Fork/Collapse strategies) */
		TSet<int32> NodesToRemove;

		/** Collapse replacement transforms (NodeIdx -> ReplacementTransform) */
		TMap<int32, FTransform> CollapseReplacements;

		/** Swap target module indices (NodeIdx -> TargetModuleIdx) */
		TMap<int32, int32> SwapTargets;

		/** Annotated node indices (all nodes that matched a pattern) */
		TSet<int32> AnnotatedNodes;

		/** Pattern name writer (for annotation) */
		TSharedPtr<PCGExData::TBuffer<FName>> PatternNameWriter;

		/** Pattern match index writer */
		TSharedPtr<PCGExData::TBuffer<int32>> PatternMatchIndexWriter;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;
		virtual void Write() override;

	protected:
		/** Find all matches for a single pattern */
		void FindMatchesForPattern(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern);

		/** Try to match a pattern starting from a specific node */
		bool TryMatchPatternFromNode(int32 PatternIndex, const FPCGExValencyPatternCompiled& Pattern, int32 StartNodeIndex, FPCGExValencyPatternMatch& OutMatch);

		/** Recursive DFS matching helper */
		bool MatchEntryRecursive(
			const FPCGExValencyPatternCompiled& Pattern,
			int32 EntryIndex,
			TArray<int32>& EntryToNode,
			TSet<int32>& UsedNodes);

		/** Resolve overlapping matches */
		void ResolveOverlaps();

		/** Apply matches to output */
		void ApplyMatches();

		/** Compute replacement transform for Collapse mode */
		FTransform ComputeReplacementTransform(const FPCGExValencyPatternMatch& Match, const FPCGExValencyPatternCompiled& Pattern);
	};

	class FBatch final : public PCGExValencyMT::TBatch<FProcessor>
	{
		/** Module data reader/writer (packed int64, owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataReader;
		TSharedPtr<PCGExData::TBuffer<int64>> ModuleDataWriter;

		/** Pattern name writer (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<FName>> PatternNameWriter;

		/** Pattern match index writer (owned here, shared with processors) */
		TSharedPtr<PCGExData::TBuffer<int32>> PatternMatchIndexWriter;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}
