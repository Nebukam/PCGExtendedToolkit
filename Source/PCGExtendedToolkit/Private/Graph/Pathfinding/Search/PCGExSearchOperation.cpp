// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

void UPCGExSearchOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

void UPCGExSearchOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
}

bool UPCGExSearchOperation::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionSettings* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionSettings* GoalSelection,
	PCGExHeuristics::THeuristicsHandler* Heuristics,
	TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback) const
{
	return false;
}
