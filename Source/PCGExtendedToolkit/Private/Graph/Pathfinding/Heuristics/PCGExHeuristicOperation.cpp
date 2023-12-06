// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicOperation.h"

#include "Data/PCGPointData.h"

void UPCGExHeuristicOperation::PrepareForData(const UPCGPointData* InSeeds, const UPCGPointData* InGoals)
{
	MaxGoalIndex = InGoals->GetPoints().Num() - 1;
}

int32 UPCGExHeuristicOperation::GetGoalIndex(const FPCGPoint& Seed, const int32 SeedIndex) const
{
	return PCGEx::SanitizeIndex(SeedIndex, MaxGoalIndex, IndexSafety);
}

void UPCGExHeuristicOperation::GetGoalIndices(const FPCGPoint& Seed, TArray<int32>& OutIndices) const
{
}

bool UPCGExHeuristicOperation::OutputMultipleGoals() const { return false; }
