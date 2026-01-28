// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicsCommon.generated.h"

UENUM(BlueprintType, meta = (DisplayName = "Heuristic Score Mode"))
enum class EPCGExHeuristicScoreMode : uint8
{
	WeightedAverage UMETA(DisplayName = "Weighted Average", Tooltip = "Balanced blending of all heuristics. Scores normalized to [0,1] range."),
	GeometricMean UMETA(DisplayName = "Geometric Mean", Tooltip = "Sensitive to extremes. A single low score significantly reduces the combined result."),
	WeightedSum UMETA(DisplayName = "Weighted Sum", Tooltip = "Direct weight contribution with no normalization. Scale varies with heuristic count."),
	HarmonicMean UMETA(DisplayName = "Harmonic Mean", Tooltip = "Heavily emphasizes low scores. Useful when lower costs should dominate the result."),
	Min UMETA(DisplayName = "Minimum", Tooltip = "Takes the minimum score across all heuristics. Most permissive - any heuristic can allow passage."),
	Max UMETA(DisplayName = "Maximum", Tooltip = "Takes the maximum score across all heuristics. Most restrictive - any heuristic can block passage."),
};

namespace PCGExHeuristics::Labels
{
	const FName SourceHeuristicsLabel = TEXT("Heuristics");
	const FName OutputHeuristicsLabel = TEXT("Heuristics");
}
