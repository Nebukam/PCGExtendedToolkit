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
	const FPCGExNodeSelectionDetails* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionDetails* GoalSelection,
	const TSharedPtr<PCGExHeuristics::THeuristicsHandler>& Heuristics,
	TArray<int32>& OutPath,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	return false;
}
