// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExLaplacianRelax.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Laplacian (Poisson)")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExLaplacianRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	virtual void ProcessExpandedNode(const PCGExCluster::FExpandedNode* ExpandedNode) override
	{
		const FVector Position = *(ReadBuffer->GetData() + ExpandedNode->Node->NodeIndex);
		FVector Force = FVector::Zero();

		for (const PCGExCluster::FExpandedNeighbor& Neighbor : ExpandedNode->Neighbors)
		{
			Force += (*(ReadBuffer->GetData() + Neighbor.Node->NodeIndex)) - Position;
		}

		(*WriteBuffer)[ExpandedNode->Node->NodeIndex] = Position + Force / static_cast<double>(ExpandedNode->Neighbors.Num());
	}
};
