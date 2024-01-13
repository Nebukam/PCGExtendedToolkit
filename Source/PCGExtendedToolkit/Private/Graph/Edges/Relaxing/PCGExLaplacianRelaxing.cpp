// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExLaplacianRelaxing.h"

#include "Graph/PCGExCluster.h"

void UPCGExLaplacianRelaxing::ProcessVertex(const PCGExCluster::FVertex& Vertex)
{
	const FVector Position = (*ReadBuffer)[Vertex.PointIndex];
	FVector Force = FVector::Zero();

	for (const int32 VtxIndex : Vertex.Neighbors)
	{
		const PCGExCluster::FVertex& OtherVtx = CurrentCluster->Vertices[VtxIndex];
		Force += (*ReadBuffer)[OtherVtx.PointIndex] - (*ReadBuffer)[Vertex.PointIndex];
	}

	(*WriteBuffer)[Vertex.PointIndex] = Position + Force / static_cast<double>(Vertex.Neighbors.Num());
}
