// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Math/PCGExMathBounds.h"

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

	/**
	 * Module indices that can match this entry (from proxied cages).
	 * Empty array = matches ANY module (wildcard behavior).
	 */
	UPROPERTY()
	TArray<int32> ModuleIndices;

	/** If true, this entry is consumed by the pattern; if false, constraint-only */
	UPROPERTY()
	bool bIsActive = true;

	/** Orbitals that must have NO neighbor (connections to null cages) */
	UPROPERTY()
	uint64 BoundaryOrbitalMask = 0;

	/** Orbitals that must have ANY neighbor (connections to wildcard cages) */
	UPROPERTY()
	uint64 WildcardOrbitalMask = 0;

	/**
	 * Connections to other pattern entries.
	 * Each FIntVector: X = TargetEntryIndex, Y = SourceOrbitalIndex, Z = TargetOrbitalIndex
	 */
	UPROPERTY()
	TArray<FIntVector> Adjacency;

	/** Check if a module index matches this entry */
	bool MatchesModule(int32 ModuleIndex) const
	{
		// Empty ModuleIndices = matches any module (wildcard)
		if (ModuleIndices.IsEmpty()) { return true; }
		return ModuleIndices.Contains(ModuleIndex);
	}

	/** Check if this entry is a wildcard (matches any module) */
	bool IsWildcard() const { return ModuleIndices.IsEmpty(); }
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

	/**
	 * Tags from the pattern root cage (actor tags).
	 * Used for pattern filtering in matchers.
	 */
	UPROPERTY()
	TArray<FName> Tags;
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

	/**
	 * Transform of the pattern root cage at compile time.
	 * Useful for rotated pattern matching - allows computing relative
	 * transforms between the authored pattern orientation and runtime matches.
	 */
	UPROPERTY()
	FTransform RootTransform = FTransform::Identity;

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

	/** Get the root node index (entry 0 maps to root) */
	int32 GetRootNodeIndex() const { return EntryToNode.IsValidIndex(0) ? EntryToNode[0] : INDEX_NONE; }

	/** Get all matched node indices (for iteration) */
	const TArray<int32>& GetMatchedNodeIndices() const { return EntryToNode; }

	/** Get the number of matched nodes */
	int32 GetMatchedNodeCount() const { return EntryToNode.Num(); }

	/**
	 * Compute axis-aligned bounds of all matched points using proper point bounds.
	 * @param PointData - Point data containing matched points
	 * @param NodeToPointIndex - Mapping from node index to point index in PointData
	 * @param BoundsSource - How to compute individual point bounds
	 * @return World-space bounds encompassing all matched point bounds
	 */
	template <typename PointAccessor>
	FBox ComputeBounds(
		const PointAccessor& Points,
		const TArray<int32>& NodeToPointIndex,
		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds) const
	{
		FBox Bounds(ForceInit);
		for (const int32 NodeIndex : EntryToNode)
		{
			if (!NodeToPointIndex.IsValidIndex(NodeIndex)) { continue; }
			const int32 PointIndex = NodeToPointIndex[NodeIndex];
			if (PointIndex == INDEX_NONE) { continue; }

			const auto& Point = Points[PointIndex];
			const FBox LocalBounds = PCGExMath::GetLocalBounds(Point, BoundsSource);
			const FTransform& Transform = Point.GetTransform();

			// Transform local bounds to world space
			Bounds += LocalBounds.TransformBy(Transform);
		}
		return Bounds;
	}

	/**
	 * Compute centroid of all matched points.
	 * @param Positions - Array of point positions indexed by point index
	 * @param NodeToPointIndex - Mapping from node index to point index
	 * @return Centroid position (world space)
	 */
	FVector ComputeCentroid(const TArray<FVector>& Positions, const TArray<int32>& NodeToPointIndex) const
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const int32 NodeIndex : EntryToNode)
		{
			if (!NodeToPointIndex.IsValidIndex(NodeIndex)) { continue; }
			const int32 PointIndex = NodeToPointIndex[NodeIndex];
			if (PointIndex == INDEX_NONE || !Positions.IsValidIndex(PointIndex)) { continue; }

			Sum += Positions[PointIndex];
			Count++;
		}
		return Count > 0 ? Sum / static_cast<double>(Count) : FVector::ZeroVector;
	}

	/**
	 * Compute centroid relative to the pattern root node's position.
	 * @param Positions - Array of point positions indexed by point index
	 * @param NodeToPointIndex - Mapping from node index to point index
	 * @return Centroid offset from root (in world space)
	 */
	FVector ComputeLocalCentroid(const TArray<FVector>& Positions, const TArray<int32>& NodeToPointIndex) const
	{
		const int32 RootNodeIndex = GetRootNodeIndex();
		if (RootNodeIndex == INDEX_NONE || !NodeToPointIndex.IsValidIndex(RootNodeIndex))
		{
			return ComputeCentroid(Positions, NodeToPointIndex);
		}

		const int32 RootPointIndex = NodeToPointIndex[RootNodeIndex];
		if (RootPointIndex == INDEX_NONE || !Positions.IsValidIndex(RootPointIndex))
		{
			return ComputeCentroid(Positions, NodeToPointIndex);
		}

		return ComputeCentroid(Positions, NodeToPointIndex) - Positions[RootPointIndex];
	}

	/**
	 * Compute bounds in a space relative to the pattern root node.
	 * @param Points - Point accessor for matched points
	 * @param NodeToPointIndex - Mapping from node index to point index
	 * @param RootTransform - Transform of the root node (from matched cluster)
	 * @param BoundsSource - How to compute individual point bounds
	 * @return Bounds in root-local space
	 */
	template <typename PointAccessor>
	FBox ComputeLocalBounds(
		const PointAccessor& Points,
		const TArray<int32>& NodeToPointIndex,
		const FTransform& RootTransform,
		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds) const
	{
		FBox Bounds(ForceInit);
		const FTransform InverseRoot = RootTransform.Inverse();

		for (const int32 NodeIndex : EntryToNode)
		{
			if (!NodeToPointIndex.IsValidIndex(NodeIndex)) { continue; }
			const int32 PointIndex = NodeToPointIndex[NodeIndex];
			if (PointIndex == INDEX_NONE) { continue; }

			const auto& Point = Points[PointIndex];
			const FBox LocalBounds = PCGExMath::GetLocalBounds(Point, BoundsSource);
			const FTransform& PointTransform = Point.GetTransform();

			// Transform point-local bounds to world, then to root-local space
			const FTransform ToRootLocal = PointTransform * InverseRoot;
			Bounds += LocalBounds.TransformBy(ToRootLocal);
		}
		return Bounds;
	}

	/**
	 * Compute the relative rotation between the authored pattern orientation
	 * and the matched runtime orientation.
	 * @param PatternRootTransform - Transform from compiled pattern (FPCGExValencyPatternCompiled::RootTransform)
	 * @param MatchedRootTransform - Transform of the matched root node at runtime
	 * @return Rotation delta to apply to pattern-space assets to align with matched orientation
	 */
	FQuat ComputePatternRotationDelta(const FTransform& PatternRootTransform, const FTransform& MatchedRootTransform) const
	{
		// Get rotation from authored pattern space to matched runtime space
		return MatchedRootTransform.GetRotation() * PatternRootTransform.GetRotation().Inverse();
	}

	/**
	 * Transform a pattern-space position to matched runtime space.
	 * Use this for assets authored relative to the pattern root.
	 * @param PatternSpacePosition - Position relative to pattern root at author time
	 * @param PatternRootTransform - Compiled pattern's root transform
	 * @param MatchedRootTransform - Transform of matched root node at runtime
	 * @return World space position in the matched cluster
	 */
	FVector TransformPatternToMatched(
		const FVector& PatternSpacePosition,
		const FTransform& PatternRootTransform,
		const FTransform& MatchedRootTransform) const
	{
		// Convert from pattern-local to matched-world space
		const FVector PatternWorld = PatternRootTransform.TransformPosition(PatternSpacePosition);
		const FVector RelativeToPatternOrigin = PatternWorld - PatternRootTransform.GetLocation();
		const FQuat RotationDelta = ComputePatternRotationDelta(PatternRootTransform, MatchedRootTransform);
		return MatchedRootTransform.GetLocation() + RotationDelta.RotateVector(RelativeToPatternOrigin);
	}
};
