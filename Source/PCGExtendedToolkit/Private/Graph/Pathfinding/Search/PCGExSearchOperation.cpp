// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"

void FPCGExSearchOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
}

bool FPCGExSearchOperation::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
	const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics,
	const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	return false;
}

TSharedPtr<PCGExPathfinding::FSearchAllocations> FPCGExSearchOperation::NewAllocations() const
{
	TSharedPtr<PCGExPathfinding::FSearchAllocations> Allocations = MakeShared<PCGExPathfinding::FSearchAllocations>();
	Allocations->Init(Cluster);
	return Allocations;
}


void UPCGExSearchInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
}
