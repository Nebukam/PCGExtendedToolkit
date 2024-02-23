// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

void UPCGExSearchOperation::PreprocessCluster(PCGExCluster::FCluster* Cluster)
{
}

bool UPCGExSearchOperation::FindPath(
	const PCGExCluster::FCluster* Cluster,
	const FVector& SeedPosition,
	const FVector& GoalPosition,
	const UPCGExHeuristicOperation* Heuristics,
	const FPCGExHeuristicModifiersSettings* Modifiers,
	TArray<int32>& OutPath,
	PCGExPathfinding::FExtraWeights* ExtraWeights)
{
	return false;
}
