// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineOperation.h"

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRefineOperation::RequiresHeuristics()
{
	return false;
}

void UPCGExEdgeRefineOperation::Process(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
}

void UPCGExEdgeRefineOperation::Cleanup()
{
	Super::Cleanup();
}
