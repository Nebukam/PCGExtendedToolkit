// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Relaxing/PCGExForceDirectedRelax.h"

#include "Graph/PCGExCluster.h"

void UPCGExForceDirectedRelax::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExForceDirectedRelax* TypedOther = Cast<UPCGExForceDirectedRelax>(Other))
	{
		SpringConstant = TypedOther->SpringConstant;
		ElectrostaticConstant = TypedOther->ElectrostaticConstant;
	}
}

void UPCGExForceDirectedRelax::ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode)
{
	const FVector Position = *(ReadBuffer->GetData() + ExpandedNode->Node->NodeIndex);
	FVector Force = FVector::Zero();

	for (const PCGExCluster::FExpandedNeighbor& Neighbor : ExpandedNode->Neighbors)
	{
		const FVector OtherPosition = *(ReadBuffer->GetData() + Neighbor.Node->NodeIndex);
		CalculateAttractiveForce(Force, Position, OtherPosition);
		CalculateRepulsiveForce(Force, Position, OtherPosition);
	}

	(*WriteBuffer)[ExpandedNode->Node->NodeIndex] = Position + Force;
}
