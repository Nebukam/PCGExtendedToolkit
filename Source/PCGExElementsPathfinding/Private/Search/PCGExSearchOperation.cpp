// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Search/PCGExSearchOperation.h"
#include "Core/PCGExSearchAllocations.h"

void FPCGExSearchOperation::PrepareForCluster(PCGExClusters::FCluster* InCluster)
{
	Cluster = InCluster;
}

bool FPCGExSearchOperation::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExPathfinding::FSearchAllocations>& Allocations,
	const TSharedPtr<PCGExHeuristics::FHandler>& Heuristics,
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
