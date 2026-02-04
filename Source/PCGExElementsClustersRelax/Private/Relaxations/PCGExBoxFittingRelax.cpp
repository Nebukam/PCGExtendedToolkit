// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relaxations/PCGExBoxFittingRelax.h"

#include "Data/PCGBasePointData.h"

#pragma region UPCGExBoxFittingRelax

bool UPCGExBoxFittingRelax::PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster)
{
	if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }
	PCGExArrayHelpers::InitArray(BoxBuffer, Cluster->Nodes->Num());
	return true;
}

EPCGExClusterElement UPCGExBoxFittingRelax::PrepareNextStep(const int32 InStep)
{
	EPCGExClusterElement Source = Super::PrepareNextStep(InStep); // Super does the buffer swap, needs to happen first
	if (InStep == 0)
	{
		const int32 NumNodes = Cluster->Nodes->Num();
		const UPCGBasePointData* InPointData = PrimaryDataFacade->GetIn();

		for (int i = 0; i < NumNodes; i++)
		{
			BoxBuffer[i] = InPointData->GetLocalBounds(Cluster->GetNodePointIndex(i)).ExpandBy(Padding).TransformBy(*(ReadBuffer->GetData() + i));
		}
	}
	return Source;
}

void UPCGExBoxFittingRelax::Step2(const PCGExClusters::FNode& Node)
{
	const FTransform& CurrentTr = *(ReadBuffer->GetData() + Node.Index);
	const FBox CurrentBox = BoxBuffer[Node.Index];
	const FVector& CurrentPos = CurrentTr.GetLocation();

	// Apply repulsion forces between all pairs of nodes
	const int32 NumNodes = Cluster->Nodes->Num();
	for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < NumNodes; OtherNodeIndex++)
	{
		const FTransform& OtherTr = *(ReadBuffer->GetData() + OtherNodeIndex);
		const PCGExClusters::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
		const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

		// Transform boxes to world space
		const FBox OtherBox = BoxBuffer[OtherNodeIndex];

		// Check for overlap
		if (!CurrentBox.Intersect(OtherBox)) { continue; }

		// Calculate overlap resolution force
		// TODO : Test with repulsion based on overlap size
		FVector Delta = OtherPos - CurrentPos;
		const double Distance = Delta.Size();

		if (Distance <= KINDA_SMALL_NUMBER) { continue; }

		// Overlap resolution
		FVector OverlapSize = CurrentBox.GetExtent() + OtherBox.GetExtent() - PCGExTypes::Abs(Delta);

		AddDelta(OtherNode->Index, Node.Index, (RepulsionConstant * OverlapSize * (Delta / Distance)));
	}
}

#pragma endregion
