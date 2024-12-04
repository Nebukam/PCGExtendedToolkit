// Copyright 2024 Timothé Lapetite and contributors
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

bool UPCGExSearchOperation::ResolveQuery(
	const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
	const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics, const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback) const
{
	return false;
}
