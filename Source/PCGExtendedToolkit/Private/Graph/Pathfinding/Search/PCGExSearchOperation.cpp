// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

void UPCGExSearchOperation::CopySettingsFrom(UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
}

bool UPCGExSearchOperation::GetRequiresProjection() { return false; }

void UPCGExSearchOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExCluster::FClusterProjection* InProjection)
{
	Cluster = InCluster;
	Projection = InProjection;
}

bool UPCGExSearchOperation::FindPath(
	const FVector& SeedPosition,
	const FPCGExNodeSelectionSettings* SeedSelection,
	const FVector& GoalPosition,
	const FPCGExNodeSelectionSettings* GoalSelection,
	PCGExHeuristics::THeuristicsHandler* Heuristics,
	TArray<int32>& OutPath, PCGExHeuristics::FLocalFeedbackHandler* LocalFeedback)
{
	return false;
}
