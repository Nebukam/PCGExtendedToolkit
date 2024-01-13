// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineOperation.h"

void UPCGExEdgeRefineOperation::PrepareForPointIO(PCGExData::FPointIO& PointIO)
{
	CurrentPoints = &PointIO;
}

void UPCGExEdgeRefineOperation::PrepareForCluster(PCGExData::FPointIO& EdgesIO, PCGExCluster::FCluster* InCluster)
{
	CurrentEdges = &EdgesIO;
	CurrentCluster = InCluster;
}

void UPCGExEdgeRefineOperation::Cleanup()
{
	CurrentPoints = nullptr;
	CurrentEdges = nullptr;
	CurrentCluster = nullptr;

	Super::Cleanup();
}
