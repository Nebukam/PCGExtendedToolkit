// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Sampling/Neighbors/PCGExNeighborSampleOperation.h"

void UPCGExNeighborSampleOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
	for (FName AId : SourceAttributes)
	{
		if (PCGExDataBlending::FDataBlendingOperationBase** BlendOp = Blender->OperationIdMap.Find(AId)) { BlendOps.Add(*BlendOp); }
	}

	if (PointFilters) { PointFilters->CaptureCluster(Context, Cluster); }
	if (UsableValueFilters) { UsableValueFilters->CaptureCluster(Context, Cluster); }
}

void UPCGExNeighborSampleOperation::ProcessNodeForPoints(const int32 InNodeIndex) const
{
	if (PointFilters && !PointFilters->Test(InNodeIndex)) { return; }

	const PCGExCluster::FNode& Node = Cluster->Nodes[InNodeIndex];

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32>* A = new TArray<int32>();
	TArray<int32>* B = new TArray<int32>();

	TArray<int32>* CurrentNeighbors = A;
	TArray<int32>* NextNeighbors = B;
	TSet<int32> VisitedNodes;

	VisitedNodes.Add(InNodeIndex);
	Cluster->GetConnectedNodes(InNodeIndex, *CurrentNeighbors, 1, VisitedNodes);

	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->PrepareOperation(InNodeIndex);
	}

	while (CurrentDepth <= MaxDepth)
	{
		CurrentDepth++;
		
		if (CurrentDepth != MaxDepth)
		{
			NextNeighbors->Empty();

			if (UsableValueFilters)
			{
				for (int i = 0; i < CurrentNeighbors->Num(); i++)
				{
					const int32 NIndex = (*CurrentNeighbors)[i];
					const int32 PtIndex = Cluster->Nodes[NIndex].PointIndex;
					if (UsableValueFilters->Test(PtIndex))
					{
						VisitedNodes.Add(NIndex);
						CurrentNeighbors->RemoveAt(i);
						i--;
					}
				}
			}

			for (const int32 Index : *CurrentNeighbors)
			{
				VisitedNodes.Add(Index);
				const PCGExCluster::FNode& NextNode = Cluster->Nodes[Index];
				double LocalWeight = 1;

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, NextNode.Position);
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
				{
					BlendOp->DoOperation(InNodeIndex, NextNode.PointIndex, InNodeIndex, LocalWeight);
				}

				Count++;
				TotalWeight += LocalWeight;
			}

			for (const int32 Index : *CurrentNeighbors)
			{
				VisitedNodes.Add(Index);
				Cluster->GetConnectedNodes(Index, *NextNeighbors, 1, VisitedNodes);
			}

			std::swap(CurrentNeighbors, NextNeighbors);
		}
		else
		{
			for (const int32 Index : *CurrentNeighbors)
			{
				const PCGExCluster::FNode& NextNode = Cluster->Nodes[Index];
				double LocalWeight = 1;

				if (UsableValueFilters && !UsableValueFilters->Test(NextNode.PointIndex)) { continue; }

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, NextNode.Position);
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
				{
					BlendOp->DoOperation(InNodeIndex, NextNode.PointIndex, InNodeIndex, LocalWeight);
				}
				Count++;
				TotalWeight += LocalWeight;
			}
		}
	}

	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->FinalizeOperation(InNodeIndex, Count, TotalWeight);
	}

	PCGEX_DELETE(A)
	PCGEX_DELETE(B)
}

void UPCGExNeighborSampleOperation::ProcessNodeForEdges(const int32 InNodeIndex) const
{
	if (PointFilters && !PointFilters->Test(InNodeIndex)) { return; }

	const PCGExCluster::FNode& Node = Cluster->Nodes[InNodeIndex];

	int32 CurrentDepth = 0;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32>* A = new TArray<int32>();
	TArray<int32>* B = new TArray<int32>();
	TArray<int32>* EA = new TArray<int32>();
	TArray<int32>* EB = new TArray<int32>();

	TArray<int32>* CurrentNeighbors = A;
	TArray<int32>* NextNeighbors = B;

	TArray<int32>* CurrentEdges = EA;
	TArray<int32>* NextEdges = EB;

	TSet<int32> VisitedNodes;
	TSet<int32> VisitedEdges;

	VisitedNodes.Add(InNodeIndex);
	Cluster->GetConnectedEdges(InNodeIndex, *CurrentNeighbors, *CurrentEdges, 1, VisitedNodes, VisitedEdges);

	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->PrepareOperation(InNodeIndex);
	}

	while (CurrentDepth <= MaxDepth)
	{
		CurrentDepth++;
		
		if (CurrentDepth != MaxDepth)
		{
			NextEdges->Empty();

			if (UsableValueFilters)
			{
				for (int i = 0; i < CurrentNeighbors->Num(); i++)
				{
					const int32 NIndex = (*CurrentNeighbors)[i];
					const int32 PtIndex = Cluster->Nodes[NIndex].PointIndex;
					if (UsableValueFilters->Test(PtIndex))
					{
						VisitedNodes.Add(NIndex);
						CurrentNeighbors->RemoveAt(i);
						i--;
					}
				}

				for (int i = 0; i < CurrentEdges->Num(); i++)
				{
					const int32 EIndex = (*CurrentEdges)[i];
					const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EIndex];
					if (!UsableValueFilters->Test(Edge.Start) || !UsableValueFilters->Test(Edge.End))
					{
						VisitedEdges.Add(EIndex);
						CurrentEdges->RemoveAt(i);
						i--;
					}
				}
			}

			for (const int32 EdgeIndex : (*CurrentEdges))
			{
				VisitedEdges.Add(EdgeIndex);
				double LocalWeight = 1;

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, Cluster->EdgesIO->GetInPoint(EdgeIndex).Transform.GetLocation());
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
				{
					BlendOp->DoOperation(InNodeIndex, EdgeIndex, InNodeIndex, LocalWeight);
				}

				Count++;
				TotalWeight += LocalWeight;
			}

			for (const int32 Index : *CurrentNeighbors) { VisitedNodes.Add(Index); }
			for (const int32 Index : *CurrentNeighbors)
			{
				Cluster->GetConnectedEdges(Index, *NextNeighbors, *NextEdges, 1, VisitedNodes, VisitedEdges);
			}

			std::swap(CurrentNeighbors, NextNeighbors);
			std::swap(CurrentEdges, NextEdges);
		}
		else
		{
			for (const int32 EdgeIndex : (*CurrentEdges))
			{
				if (UsableValueFilters)
				{
					const PCGExGraph::FIndexedEdge& Edge = Cluster->Edges[EdgeIndex];
					if (!UsableValueFilters->Test(Edge.Start) || !UsableValueFilters->Test(Edge.End)) { continue; }
				}

				double LocalWeight = 1;

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, Cluster->EdgesIO->GetInPoint(EdgeIndex).Transform.GetLocation());
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
				{
					BlendOp->DoOperation(InNodeIndex, EdgeIndex, InNodeIndex, LocalWeight);
				}

				Count++;
				TotalWeight += LocalWeight;
			}
		}

	}

	for (const PCGExDataBlending::FDataBlendingOperationBase* BlendOp : BlendOps)
	{
		BlendOp->FinalizeOperation(InNodeIndex, Count, TotalWeight);
	}
}

void UPCGExNeighborSampleOperation::Cleanup()
{
	BlendOps.Empty();
	Super::Cleanup();
}

double UPCGExNeighborSampleOperation::SampleCurve(const double InTime) const
{
	return 1;
}
