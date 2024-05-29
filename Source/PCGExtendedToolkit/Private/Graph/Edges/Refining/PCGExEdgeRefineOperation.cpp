// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineOperation.h"

#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeRefineOperation::PrepareForPointIO(PCGExData::FPointIO* InPointIO)
{
	PointIO = InPointIO;
}

void UPCGExEdgeRefineOperation::PreProcess(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO)
{
}

void UPCGExEdgeRefineOperation::Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
}

void UPCGExEdgeRefineOperation::Cleanup()
{
	PointIO = nullptr;
	Super::Cleanup();
}
