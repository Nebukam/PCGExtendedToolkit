// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExValencyPattern.generated.h"

/**
 * Pattern output strategy - how matched points are processed.
 */
UENUM(BlueprintType)
enum class EPCGExPatternOutputStrategy : uint8
{
	Remove UMETA(ToolTip = "Remove matched points from main output, output to secondary pin"),
	Collapse UMETA(ToolTip = "Collapse N matched points into 1 replacement point"),
	Swap UMETA(ToolTip = "Swap matched points to different modules"),
	Annotate UMETA(ToolTip = "Annotate matched points with metadata, no removal"),
	Fork UMETA(ToolTip = "Fork matched points to separate collection for parallel processing")
};

/**
 * Transform mode for Collapse output strategy.
 */
UENUM(BlueprintType)
enum class EPCGExPatternTransformMode : uint8
{
	Centroid UMETA(ToolTip = "Compute centroid of all matched points"),
	PatternRoot UMETA(ToolTip = "Use pattern root cage's position"),
	FirstMatch UMETA(ToolTip = "Use first matched point's transform")
};

/**
 * Compiled entry within a pattern.
 * Each entry represents one position in the pattern topology.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternEntryCompiled
{
	GENERATED_BODY()

	/** Module indices that can match this entry (from proxied cages) */
	UPROPERTY()
	TArray<int32> ModuleIndices;

	/** If true, matches any module (ignores ModuleIndices) */
	UPROPERTY()
	bool bIsWildcard = false;

	/** If true, this entry is consumed by the pattern; if false, constraint-only */
	UPROPERTY()
	bool bIsActive = true;

	/** Orbitals that must have NO neighbor (connections to null cages) */
	UPROPERTY()
	uint64 BoundaryOrbitalMask = 0;

	/**
	 * Connections to other pattern entries.
	 * Each FIntVector: X = TargetEntryIndex, Y = SourceOrbitalIndex, Z = TargetOrbitalIndex
	 */
	UPROPERTY()
	TArray<FIntVector> Adjacency;

	/** Check if a module index matches this entry */
	bool MatchesModule(int32 ModuleIndex) const
	{
		if (bIsWildcard) { return true; }
		return ModuleIndices.Contains(ModuleIndex);
	}
};

/**
 * Compiled pattern settings (mirrors FPCGExValencyPatternSettings from editor).
 * Stored in the compiled pattern for runtime access.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternSettingsCompiled
{
	GENERATED_BODY()

	/** Pattern name for identification */
	UPROPERTY()
	FName PatternName;

	/** Weight for probabilistic selection among competing patterns */
	UPROPERTY()
	float Weight = 1.0f;

	/** Minimum times this pattern must be matched (0 = no minimum) */
	UPROPERTY()
	int32 MinMatches = 0;

	/** Maximum times this pattern can be matched (-1 = unlimited) */
	UPROPERTY()
	int32 MaxMatches = -1;

	/** If true, matched points are claimed exclusively (removed from main output) */
	UPROPERTY()
	bool bExclusive = true;

	/** Output strategy for matched points */
	UPROPERTY()
	EPCGExPatternOutputStrategy OutputStrategy = EPCGExPatternOutputStrategy::Remove;

	/** Transform computation mode for Collapse strategy */
	UPROPERTY()
	EPCGExPatternTransformMode TransformMode = EPCGExPatternTransformMode::Centroid;
};

/**
 * Compiled pattern for runtime matching.
 * Contains the pattern topology and all data needed for subgraph matching.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternCompiled
{
	GENERATED_BODY()

	/** Pattern entries (index 0 = root) */
	UPROPERTY()
	TArray<FPCGExValencyPatternEntryCompiled> Entries;

	/** Pattern settings */
	UPROPERTY()
	FPCGExValencyPatternSettingsCompiled Settings;

	/** Replacement asset for Collapse mode */
	UPROPERTY()
	TSoftObjectPtr<UObject> ReplacementAsset;

	/** Swap target module index for Swap mode (-1 = invalid/unresolved) */
	UPROPERTY()
	int32 SwapTargetModuleIndex = -1;

	/** Number of active entries (entries that consume points) */
	UPROPERTY()
	int32 ActiveEntryCount = 0;

	/** Get the number of entries in this pattern */
	int32 GetEntryCount() const { return Entries.Num(); }

	/** Check if this pattern is valid (has at least one entry) */
	bool IsValid() const { return Entries.Num() > 0; }
};

/**
 * Compiled set of all patterns from a BondingRules asset.
 * Organized for efficient runtime matching.
 */
USTRUCT()
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternSetCompiled
{
	GENERATED_BODY()

	/** All compiled patterns */
	UPROPERTY()
	TArray<FPCGExValencyPatternCompiled> Patterns;

	/** Indices of exclusive patterns (must be processed first, in order) */
	UPROPERTY()
	TArray<int32> ExclusivePatternIndices;

	/** Indices of additive patterns (can be processed after exclusive) */
	UPROPERTY()
	TArray<int32> AdditivePatternIndices;

	/** Check if there are any patterns */
	bool HasPatterns() const { return Patterns.Num() > 0; }

	/** Get total pattern count */
	int32 GetPatternCount() const { return Patterns.Num(); }
};

/**
 * A single pattern match found during runtime matching.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExValencyPatternMatch
{
	/** Index of the matched pattern in PatternSetCompiled */
	int32 PatternIndex = -1;

	/** Mapping: pattern entry index -> node index in solved cluster */
	TArray<int32> EntryToNode;

	/** Computed replacement transform (for Collapse mode) */
	FTransform ReplacementTransform = FTransform::Identity;

	/** Whether this match has been claimed (for exclusive patterns) */
	bool bClaimed = false;

	/** Check if this match is valid */
	bool IsValid() const { return PatternIndex >= 0 && EntryToNode.Num() > 0; }
};
