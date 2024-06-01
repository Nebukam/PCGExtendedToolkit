// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Sampling/Neighbors/PCGExNeighborSampleOperation.h"

void UPCGExNeighborSampleOperation::PrepareForCluster(PCGExCluster::FCluster* InCluster)
{
	Cluster = InCluster;
	BlendOp = *Blender->OperationIdMap.Find(SourceAttribute);
}

void UPCGExNeighborSampleOperation::ProcessNodeForPoints(
	const PCGEx::FPointRef& Target,
	const PCGExCluster::FNode& Node) const
{
	int32 CurrentDepth = 1;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32> CurrentNeighbors;
	TArray<int32> NextNeighbors;
	TSet<int32> VisitedNodes;

	VisitedNodes.Add(Node.NodeIndex);
	GrabNeighbors(Node, CurrentNeighbors, VisitedNodes);

	BlendOp->PrepareOperation(Target.Index);

	while (CurrentDepth <= MaxDepth)
	{
		if (CurrentDepth != MaxDepth)
		{
			for (const int32 Index : CurrentNeighbors)
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

				BlendOp->DoOperation(Target.Index, NextNode.PointIndex, Target.Index, LocalWeight);
				Count++;
				TotalWeight += LocalWeight; // TODO: Implement proper weight computation
			}

			for (const int32 Index : CurrentNeighbors)
			{
				VisitedNodes.Add(Index);
				GrabNeighbors(Cluster->Nodes[Index], NextNeighbors, VisitedNodes);
			}

			//TODO : Refacto to swap
			CurrentNeighbors.Empty();
			CurrentNeighbors.Append(NextNeighbors);
			NextNeighbors.Empty();
		}
		else
		{
			for (const int32 Index : CurrentNeighbors)
			{
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

				BlendOp->DoOperation(Target.Index, NextNode.PointIndex, Target.Index, LocalWeight);
				Count++;
				TotalWeight += LocalWeight; // TODO: Implement proper weight computation
			}
		}

		CurrentDepth++;
	}

	BlendOp->FinalizeOperation(Target.Index, Count, TotalWeight);
}

void UPCGExNeighborSampleOperation::ProcessNodeForEdges(
	const PCGEx::FPointRef& Target,
	const PCGExCluster::FNode& Node) const
{

	int32 CurrentDepth = 1;
	int32 Count = 0;
	double TotalWeight = 0;

	TArray<int32> CurrentNeighbors;
	TArray<int32> CurrentEdges;
	TArray<int32> NextNeighbors;
	TSet<int32> VisitedNodes;
	TSet<int32> VisitedEdges;

	VisitedNodes.Add(Node.NodeIndex);
	GrabNeighborsEdges(Node, CurrentNeighbors, CurrentEdges, VisitedNodes, VisitedEdges);

	BlendOp->PrepareOperation(Target.Index);

	while (CurrentDepth <= MaxDepth)
	{
		if (CurrentDepth != MaxDepth)
		{
			for (const int32 EdgeIndex : CurrentEdges)
			{
				VisitedEdges.Add(EdgeIndex);
				const PCGEx::FPointRef TargetEdge = Cluster->EdgesIO->GetInPointRef(EdgeIndex);
				double LocalWeight = 1;

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, TargetEdge.Point->Transform.GetLocation());
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				BlendOp->DoOperation(Target.Index, TargetEdge.Index, Target.Index, LocalWeight);
				Count++;
				TotalWeight += LocalWeight; // TODO: Implement proper weight computation
			}

			for (const int32 Index : CurrentNeighbors)
			{
				const PCGExCluster::FNode& NextNode = Cluster->Nodes[Index];
				GrabNeighborsEdges(NextNode, NextNeighbors, CurrentEdges, VisitedNodes, VisitedEdges);
			}

			//TODO : Refacto to swap
			CurrentNeighbors.Empty();
			CurrentEdges.Empty();
			CurrentNeighbors.Append(NextNeighbors);
			NextNeighbors.Empty();
		}
		else
		{
			for (const int32 EdgeIndex : CurrentEdges)
			{
				const PCGEx::FPointRef TargetEdge = Cluster->EdgesIO->GetInPointRef(EdgeIndex);
				double LocalWeight = 1;

				if (BlendOver == EPCGExBlendOver::Distance)
				{
					const double Dist = FVector::Distance(Node.Position, TargetEdge.Point->Transform.GetLocation());
					if (Dist > MaxDistance) { continue; }
					LocalWeight = 1 - (Dist / MaxDistance);
				}
				else
				{
					LocalWeight = BlendOver == EPCGExBlendOver::Distance ? 1 - (CurrentDepth / (MaxDepth - 1)) : FixedBlend;
				}

				LocalWeight = WeightCurveObj->GetFloatValue(LocalWeight);

				BlendOp->DoOperation(Target.Index, TargetEdge.Index, Target.Index, LocalWeight);
				Count++;
				TotalWeight += LocalWeight; // TODO: Implement proper weight computation
			}
		}

		CurrentDepth++;
	}

	BlendOp->FinalizeOperation(Target.Index, Count, TotalWeight);
}

void UPCGExNeighborSampleOperation::Cleanup()
{
	Super::Cleanup();
}

double UPCGExNeighborSampleOperation::SampleCurve(const double InTime) const
{
	return 1;
}

void UPCGExNeighborSampleOperation::GrabNeighbors(
	const PCGExCluster::FNode& From,
	TArray<int32>& OutNeighbors,
	const TSet<int32>& Visited) const
{
	for (const int32 OtherNodeIndex : From.AdjacentNodes)
	{
		if (Visited.Contains(OtherNodeIndex)) { continue; }
		OutNeighbors.AddUnique(Cluster->Nodes[OtherNodeIndex].NodeIndex);
	}
}

void UPCGExNeighborSampleOperation::GrabNeighborsEdges(
	const PCGExCluster::FNode& From,
	TArray<int32>& OutNeighbors,
	TArray<int32>& OutNeighborsEdges,
	const TSet<int32>& Visited,
	const TSet<int32>& VisitedEdges) const
{
	for (const int32 OtherNodeIndex : From.AdjacentNodes)
	{
		if (Visited.Contains(OtherNodeIndex)) { continue; }
		OutNeighbors.AddUnique(Cluster->Nodes[OtherNodeIndex].NodeIndex);
		const PCGExGraph::FIndexedEdge& Edge = Cluster->GetEdgeFromNodeIndices(From.NodeIndex, OtherNodeIndex);
		if (VisitedEdges.Contains(Edge.PointIndex))
			OutNeighborsEdges.AddUnique(Edge.PointIndex);
	}
}
