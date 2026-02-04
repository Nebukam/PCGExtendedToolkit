// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Refinements/PCGExEdgeRefineRemoveLeaves.h"

#pragma region FPCGExEdgeRemoveLeaves

void FPCGExEdgeRemoveLeaves::ProcessNode(PCGExClusters::FNode& Node)
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

#pragma endregion
