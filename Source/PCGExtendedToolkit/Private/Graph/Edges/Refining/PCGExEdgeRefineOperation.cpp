// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeRefineOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	Cluster = InCluster;
	Heuristics = InHeuristics;

	if (RequiresNodeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Node); }
	if (RequiresEdgeOctree()) { Cluster->RebuildOctree(EPCGExClusterClosestSearchMode::Edge); }
}

void UPCGExEdgeRefineOperation::Cleanup()
{
	Super::Cleanup();
}
