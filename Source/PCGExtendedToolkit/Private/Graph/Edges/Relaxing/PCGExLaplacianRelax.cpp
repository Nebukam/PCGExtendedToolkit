// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExLaplacianRelax.h"

#include "Graph/PCGExCluster.h"

void UPCGExLaplacianRelax::ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode)
{
	const FVector Position = *(ReadBuffer->GetData() + ExpandedNode->Node->NodeIndex);
	FVector Force = FVector::Zero();

	for (const PCGExCluster::FExpandedNeighbor& Neighbor : ExpandedNode->Neighbors)
	{
		Force += (*(ReadBuffer->GetData() + Neighbor.Node->NodeIndex)) - Position;
	}

	(*WriteBuffer)[ExpandedNode->Node->NodeIndex] = Position + Force / static_cast<double>(ExpandedNode->Neighbors.Num());
}
