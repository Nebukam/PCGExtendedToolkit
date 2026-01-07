// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Clusters/PCGExCluster.h"
#include "PCGExEdgeRefineRemoveLeaves.generated.h"

/**
 * 
 */
class FPCGExEdgeRemoveLeaves : public FPCGExEdgeRefineOperation
{
public:
	virtual void ProcessNode(PCGExClusters::FNode& Node) override
	{
		int32 CurrentNodeIndex = Node.Index;
		int32 PrevNodeIndex = -1;

		if (!Node.IsLeaf()) { return; }

		while (CurrentNodeIndex != -1)
		{
			PCGExClusters::FNode* From = Cluster->GetNode(CurrentNodeIndex);

			if (From->IsComplex()) { return; }

			if (PrevNodeIndex == CurrentNodeIndex) { return; }

			From->bValid = false;
			PrevNodeIndex = CurrentNodeIndex;
			CurrentNodeIndex = Node.Links[0].Node;

			Cluster->GetEdge(Node.Links[0])->bValid = false;
		}
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove Leaves", PCGExNodeLibraryDoc="clusters/refine-cluster/remove-leaves"))
class UPCGExEdgeRemoveLeaves : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool WantsIndividualNodeProcessing() const override { return true; }

	PCGEX_CREATE_REFINE_OPERATION(EdgeRemoveLeaves, {})
};
