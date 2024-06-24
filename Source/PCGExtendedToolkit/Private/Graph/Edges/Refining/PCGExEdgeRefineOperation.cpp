// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineOperation.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExCluster.h"

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

bool UPCGExEdgeRefineOperation::RequiresHeuristics()
{
	return false;
}

bool UPCGExEdgeRefineOperation::RequiresIndividualNodeProcessing()
{
	return false;
}

bool UPCGExEdgeRefineOperation::RequiresIndividualEdgeProcessing()
{
	return false;
}

void UPCGExEdgeRefineOperation::Process(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
}

void UPCGExEdgeRefineOperation::ProcessNode(PCGExCluster::FNode& Node, PCGExCluster::FCluster* InCluster, FRWLock& NodeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
}

void UPCGExEdgeRefineOperation::ProcessEdge(PCGExGraph::FIndexedEdge& Edge, PCGExCluster::FCluster* InCluster, FRWLock& EdgeLock, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
}

void UPCGExEdgeRefineOperation::Cleanup()
{
	Super::Cleanup();
}
