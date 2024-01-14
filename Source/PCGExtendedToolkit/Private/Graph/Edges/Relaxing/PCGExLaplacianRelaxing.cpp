// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExLaplacianRelaxing.h"

#include "Graph/PCGExCluster.h"

void UPCGExLaplacianRelaxing::ProcessVertex(const PCGExCluster::FNode& Vertex)
{
	const FVector Position = (*ReadBuffer)[Vertex.PointIndex];
	FVector Force = FVector::Zero();

	for (const int32 VtxIndex : Vertex.AdjacentNodes)
	{
		const PCGExCluster::FNode& OtherVtx = CurrentCluster->Nodes[VtxIndex];
		Force += (*ReadBuffer)[OtherVtx.PointIndex] - (*ReadBuffer)[Vertex.PointIndex];
	}

	(*WriteBuffer)[Vertex.PointIndex] = Position + Force / static_cast<double>(Vertex.AdjacentNodes.Num());
}
